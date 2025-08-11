#include "file_indexer.h"
#include <iostream>
#include <fstream>
#include <algorithm>
#include <cctype>
#include <zstd.h>
#include <future>
#include <chrono>

using json = nlohmann::json;

#define COMPRESSION_LEVEL 1

// ---------------- JSON Serialization ----------------

void IndexedFile::to_json(json& j) const {
    j = json{
        {"name", name},
        {"path", path},
        {"size", size},
        {"extension", extension},
        {"last_modified", std::chrono::duration_cast<std::chrono::seconds>(
            last_modified.time_since_epoch()).count()}
    };
}

void IndexedFile::from_json(const json& j) {
    name = j.at("name").get<std::string>();
    path = j.at("path").get<std::string>();
    size = j.at("size").get<std::uintmax_t>();
    extension = j.value("extension", "");
    auto secs = j.at("last_modified").get<uint64_t>();
    last_modified = std::filesystem::file_time_type(std::chrono::seconds(secs));
}

void IndexedDirectory::to_json(json& j) const {
    j = json{
        {"name", name},
        {"path", path},
        {"size", size},
        {"last_modified", std::chrono::duration_cast<std::chrono::seconds>(
            last_modified.time_since_epoch()).count()}
    };
}

void IndexedDirectory::from_json(const json& j) {
    name = j.at("name").get<std::string>();
    path = j.at("path").get<std::string>();
    size = j.at("size").get<std::uintmax_t>();
    auto secs = j.at("last_modified").get<uint64_t>();
    last_modified = std::filesystem::file_time_type(std::chrono::seconds(secs));
}

// ---------------- FileIndexer Implementation ----------------

namespace FileIndexer {
    namespace {
        std::vector<IndexedFile> file_index;
        std::vector<IndexedDirectory> dir_index;

        std::thread index_thread;
        std::atomic<bool> indexing{false};
        std::mutex index_mutex;
    }

    void IndexDirectory(const std::filesystem::path& directory,
                        std::vector<IndexedFile>& files_out,
                        std::vector<IndexedDirectory>& dirs_out) {
        try {
            for (const auto& entry : std::filesystem::directory_iterator(directory)) {
                if (!indexing) return;

                if (entry.is_directory()) {
                    IndexedDirectory dir;
                    dir.name = entry.path().filename().string();
                    dir.path = entry.path().string();
                    dir.size = 0; // Optional: could calculate total size if needed
                    dir.last_modified = entry.last_write_time();
                    dirs_out.push_back(dir);

                    // Recursive call
                    IndexDirectory(entry.path(), files_out, dirs_out);

                } else if (entry.is_regular_file()) {
                    IndexedFile file;
                    file.name = entry.path().filename().string();
                    file.path = entry.path().string();
                    file.size = entry.file_size();
                    file.extension = entry.path().extension().string();
                    file.last_modified = entry.last_write_time();
                    files_out.push_back(file);
                }
            }
        } catch (const std::exception& e) {
            std::cerr << "Indexing error: " << e.what() << "\n";
        }
    }

    void IndexAsync(const std::string& root) {
        std::vector<IndexedFile> temp_files;
        std::vector<IndexedDirectory> temp_dirs;

        IndexDirectory(root, temp_files, temp_dirs);

        {
            std::lock_guard<std::mutex> lock(index_mutex);
            file_index = std::move(temp_files);
            dir_index = std::move(temp_dirs);
        }
        indexing = false;
    }

    void StartIndexing(const std::string& directory) {
        if (indexing) return;

        indexing = true;

        if (index_thread.joinable())
            index_thread.join();

        {
            std::lock_guard<std::mutex> lock(index_mutex);
            file_index.clear();
            dir_index.clear();
        }

        index_thread = std::thread([directory]() {
            IndexAsync(directory);
            std::cout << "indexing async: " << directory << std::endl;
        });
    }

    bool IsIndexing() {
        return indexing;
    }

    const std::vector<IndexedFile>& GetIndex() {
        return file_index;
    }

    const std::vector<IndexedDirectory>& GetDirectoryIndex() {
        return dir_index;
    }

    std::vector<IndexedFile> Search(const std::string& query, bool include_dirs) {
        std::vector<IndexedFile> results;
        std::lock_guard<std::mutex> lock(index_mutex);

        auto to_lower = [](const std::string& s) {
            std::string lower;
            std::transform(s.begin(), s.end(), std::back_inserter(lower),
                [](unsigned char c) { return std::tolower(c); });
            return lower;
        };

        std::string lower_query = to_lower(query);

        for (const auto& file : file_index) {
            if (to_lower(file.name).find(lower_query) != std::string::npos) {
                results.push_back(file);
            }
        }

        if (include_dirs) {
            for (const auto& dir : dir_index) {
                if (to_lower(dir.name).find(lower_query) != std::string::npos) {
                    // Convert IndexedDirectory to IndexedFile-like for unified return
                    IndexedFile f;
                    f.name = dir.name;
                    f.path = dir.path;
                    f.size = dir.size;
                    f.extension = ""; // directories have no extension
                    f.last_modified = dir.last_modified;
                    results.push_back(std::move(f));
                }
            }
        }

        return results;
    }

    void SaveToFile(const std::string& path) {
        std::lock_guard<std::mutex> lock(index_mutex);

        json j;
        j["files"] = json::array();
        j["dirs"] = json::array();

        for (const auto& file : file_index) {
            json f;
            file.to_json(f);
            j["files"].push_back(f);
        }
        for (const auto& dir : dir_index) {
            json d;
            dir.to_json(d);
            j["dirs"].push_back(d);
        }

        std::string fullPath = path + "/.index";
        std::ofstream out(fullPath, std::ios::binary);
        if (!out.is_open()) {
            std::cerr << "Failed to save index to " << fullPath << "\n";
            return;
        }

        std::string jsonStr = j.dump(2);
        out.write(jsonStr.data(), jsonStr.size());
        out.close();

        // Compress to .zst
        std::ifstream inFile(fullPath, std::ios::binary | std::ios::ate);
        if (!inFile) {
            std::cerr << "Failed to open file for compression\n";
            return;
        }
        size_t size = (size_t)inFile.tellg();
        inFile.seekg(0);
        std::vector<char> input(size);
        inFile.read(input.data(), size);

        size_t bound = ZSTD_compressBound(size);
        std::vector<char> compressed(bound);

        size_t compSize = ZSTD_compress(compressed.data(), bound, input.data(), size, COMPRESSION_LEVEL);
        if (ZSTD_isError(compSize)) {
            std::cerr << "Compression error: " << ZSTD_getErrorName(compSize) << "\n";
            return;
        }

        std::ofstream outFile(fullPath + ".zst", std::ios::binary);
        outFile.write(compressed.data(), compSize);
        outFile.close();

        std::cout << "Index saved and compressed to " << fullPath << ".zst\n";
    }

    bool LoadFromFile(const std::string& path) {
        std::string jsonFilePath = path + "/.index";

        std::ifstream in(jsonFilePath, std::ios::binary);
        if (!in.is_open()) {
            std::cerr << "Failed to open index file " << jsonFilePath << "\n";
            return false;
        }

        json j;
        try {
            in >> j;
        } catch (const std::exception& e) {
            std::cerr << "JSON parsing error: " << e.what() << "\n";
            return false;
        }

        std::vector<IndexedFile> temp_files;
        std::vector<IndexedDirectory> temp_dirs;

        for (const auto& item : j["files"]) {
            IndexedFile f;
            f.from_json(item);
            temp_files.push_back(std::move(f));
        }
        for (const auto& item : j["dirs"]) {
            IndexedDirectory d;
            d.from_json(item);
            temp_dirs.push_back(std::move(d));
        }

        {
            std::lock_guard<std::mutex> lock(index_mutex);
            file_index = std::move(temp_files);
            dir_index = std::move(temp_dirs);
        }

        std::cout << "Loaded index from " << jsonFilePath << "\n";
        return true;
    }

    std::vector<IndexedFile> ShowFilesInTab(const std::string& path) {
        if (LoadFromFile(path)) {
            return Search("", true);
        } else {
            StartIndexing(path);

            // Non-blocking: return empty for now; indexing continues async
            return {};
        }
    }

    std::tuple<std::vector<IndexedDirectory>, std::vector<IndexedFile>> ShowFilesAndDirsInTab(const std::string& path) {
        if (LoadFromFile(path)) {
            std::lock_guard<std::mutex> lock(index_mutex);
            return { dir_index, file_index };
        } else {
            // Start indexing synchronously/blocking
            std::vector<IndexedFile> files;
            std::vector<IndexedDirectory> dirs;
    
            indexing = true;
            IndexDirectory(path, files, dirs);  // Synchronous indexing
    
            {
                std::lock_guard<std::mutex> lock(index_mutex);
                file_index = std::move(files);
                dir_index = std::move(dirs);
            }
            indexing = false;
    
            SaveToFile(path);  // Optionally save after indexing
    
            return { dir_index, file_index };
        }
    }
    
    
    void Shutdown() {
        indexing = false;
        if (index_thread.joinable()) {
            index_thread.join();
        }
    }
}
