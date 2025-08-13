#include "file_indexer.h"
#include <iostream>
#include <fstream>
#include <algorithm>
#include <cctype>
#include <zstd.h>
#include <future>
#include <chrono>
#include <sys/stat.h>
#include <unordered_map>
#include <chrono>

using json = nlohmann::json;

#define COMPRESSION_LEVEL 1
#define DEBUG_MESURE_TIMES 1

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
        std::unordered_map<std::string, IndexedFile> file_index;
        std::unordered_map<std::string, IndexedDirectory> dir_index;

        std::thread index_thread;
        std::atomic<bool> indexing{false};
        std::mutex index_mutex;
    }

    void IndexDirectory(const std::filesystem::path& directory,
        std::unordered_map<std::string, IndexedFile>& files_out,
        std::unordered_map<std::string, IndexedDirectory>& dirs_out) {
#if DEBUG_MESURE_TIMES
        std::chrono::steady_clock::time_point clock_begin = std::chrono::steady_clock::now();
#endif  
        int files_count = 0;
        int dirs_count = 0;
        try {
            for (const auto& entry : std::filesystem::directory_iterator(directory)) {
                if (!indexing) return;

                if (entry.is_directory()) {
                    IndexedDirectory dir;
                    dir.name = entry.path().filename().string();
                    dir.path = entry.path().string();
                    dir.size = GetDirectorySize(entry.path());
                    dir.last_modified = entry.last_write_time();
                    dirs_out[dir.path] = dir;
                    dirs_count++;

                } else if (entry.is_regular_file()) {
                    IndexedFile file;
                    file.name = entry.path().filename().string();
                    file.path = entry.path().string();
                    file.size = entry.file_size();
                    file.extension = entry.path().extension().string();
                    file.last_modified = entry.last_write_time();
                    files_out[file.path] = file;
                    files_count++;
                }
            }
        }
        catch (const std::exception& e) {
            std::cerr << "Indexing error: " << e.what() << "\n";
        }
        indexing = false;
        //std::cout << "Indexed " << files_count << " files and " << dirs_count << " directories in " << directory << "\n";
#if DEBUG_MESURE_TIMES
        std::chrono::steady_clock::time_point clock_end = std::chrono::steady_clock::now();

        std::chrono::steady_clock::duration time_span = clock_end - clock_begin;

        double nseconds = double(time_span.count()) * std::chrono::steady_clock::period::num / std::chrono::steady_clock::period::den;

        std::cout << "It took me " << nseconds << " seconds.\n";
#endif
    }

    long GetFileSize(std::string filename)
    {
        struct stat stat_buf;
        int rc = stat(filename.c_str(), &stat_buf);
        return rc == 0 ? stat_buf.st_size : -1;
    }

    //TODO: optimise this shit
    std::uintmax_t GetDirectorySize(const std::filesystem::path& dir) {
        std::uintmax_t total_size = 0;

        try {
            for (const auto& entry : std::filesystem::recursive_directory_iterator(dir)) {
                if (entry.is_regular_file()) {
                    total_size += entry.file_size();
                    //std::cout << total_size << std::endl;
                }
            }
        }
        catch (const std::exception& e) {
            std::cerr << "Error calculating size for " << dir << ": " << e.what() << '\n';
        }

        return total_size;
    }


    void IndexAsync(const std::string& root) {
        std::unordered_map<std::string, IndexedFile> temp_files;
        std::unordered_map<std::string, IndexedDirectory> temp_dirs;

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

    const std::unordered_map<std::string, IndexedFile>& GetIndex() {
        return file_index;
    }

    const std::unordered_map<std::string, IndexedDirectory>& GetDirectoryIndex() {
        return dir_index;
    }

    std::string HumanReadableSize(std::uintmax_t size) {
        const char* suffixes[] = { "B", "KB", "MB", "GB", "TB" };
        int i = 0;
        double dblSize = static_cast<double>(size);

        while (dblSize >= 1024 && i < 4) {
            dblSize /= 1024;
            ++i;
        }

        char buf[64];
        std::snprintf(buf, sizeof(buf), "%.2f %s", dblSize, suffixes[i]);
        return std::string(buf);
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

        for (const auto& [key, file] : file_index) {
            if (to_lower(file.name).find(lower_query) != std::string::npos) {
                results.push_back(file);
            }
        }

        if (include_dirs) {
            for (const auto& [key, dir] : dir_index) {
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

        if (file_index.empty() && dir_index.empty()) {
            std::cerr << "SaveToFile: Skipping save because index is empty.\n";
            return;
        }

        json j;
        j["files"] = json::array();
        j["dirs"] = json::array();

        for (const auto& [key, file] : file_index) {
            json f;
            file.to_json(f);
            j["files"].push_back(f);
        }
        for (const auto& [key, dir] : dir_index) {
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
        // Delete original .index file after compression
        if (std::remove(fullPath.c_str()) != 0) {
            std::cerr << "Failed to delete temporary .index file\n";
        }
        else {
            std::cout << "Deleted temporary .index file\n";
        }

    }

    bool LoadFromFile(const std::string& path) {
        std::string jsonFilePath = path + "/.index";
        std::ifstream in(jsonFilePath, std::ios::binary);

        if (!in.is_open()) {
            std::cerr << "Failed to open index file " << jsonFilePath << ", trying compressed version...\n";

            // Try to decompress .zst
            std::ifstream zstIn(jsonFilePath + ".zst", std::ios::binary | std::ios::ate);
            if (!zstIn.is_open()) {
                std::cerr << "No compressed .zst index found either.\n";
                return false;
            }

            size_t size = zstIn.tellg();
            zstIn.seekg(0);
            std::vector<char> compressed(size);
            zstIn.read(compressed.data(), size);

            unsigned long long const rSize = ZSTD_getFrameContentSize(compressed.data(), size);
            if (rSize == ZSTD_CONTENTSIZE_ERROR || rSize == ZSTD_CONTENTSIZE_UNKNOWN) {
                std::cerr << "Invalid or unknown compressed content size\n";
                return false;
            }

            std::vector<char> decompressed(rSize);
            size_t dSize = ZSTD_decompress(decompressed.data(), rSize, compressed.data(), size);
            if (ZSTD_isError(dSize)) {
                std::cerr << "Decompression failed: " << ZSTD_getErrorName(dSize) << "\n";
                return false;
            }

            // Parse the decompressed string
            try {
                json j = json::parse(decompressed.begin(), decompressed.begin() + dSize);

                std::unordered_map<std::string, IndexedFile> temp_files;
                std::unordered_map<std::string, IndexedDirectory> temp_dirs;

                for (const auto& f : j["files"]) {
                    IndexedFile file;
                    file.from_json(f);
                    temp_files[file.path + "/" + file.name] = file;
                }
                for (const auto& d : j["dirs"]) {
                    IndexedDirectory dir;
                    dir.from_json(d);
                    temp_dirs[dir.path + "/" + dir.name] = dir;
                }

                {
                    std::lock_guard<std::mutex> lock(index_mutex);
                    file_index = std::move(temp_files);
                    dir_index = std::move(temp_dirs);
                }
                std::cout << "Loaded index from compressed file\n";
                return true;
            }
            catch (const std::exception& e) {
                std::cerr << "Failed to parse JSON from decompressed data: " << e.what() << "\n";
                return false;
            }
        }

        // If regular file opened successfully:
        try {
            json j;
            in >> j;

            std::unordered_map<std::string, IndexedFile> temp_files;
            std::unordered_map<std::string, IndexedDirectory> temp_dirs;

            for (const auto& f : j["files"]) {
                IndexedFile file;
                file.from_json(f);
                temp_files[file.path + "/" + file.name] = file;
            }
            for (const auto& d : j["dirs"]) {
                IndexedDirectory dir;
                dir.from_json(d);
                temp_dirs[dir.path + "/" + dir.name] = dir;
            }

            {
                std::lock_guard<std::mutex> lock(index_mutex);
                file_index = std::move(temp_files);
                dir_index = std::move(temp_dirs);
            }

            std::cout << "Loaded index from file\n";
            return true;
        }
        catch (const std::exception& e) {
            std::cerr << "Failed to load index from file: " << e.what() << "\n";
            return false;
        }
    }


    std::tuple<std::unordered_map<std::string, IndexedDirectory>, std::unordered_map<std::string, IndexedFile>> ShowFilesAndDirsContinous(const std::string& path) {
        indexing = true;

        LoadFromFile(path); // load from saved .index if exists

        std::unordered_map<std::string, IndexedFile> files;
        std::unordered_map<std::string, IndexedDirectory> dirs;

        IndexDirectory(path, files, dirs);  // Synchronous indexing
        //std::cout << "ShowFilesAndDirsContinous: Indexed " << files.size() << " files and " << dirs.size() << " directories in " << path << "\n";

        {
            std::lock_guard<std::mutex> lock(index_mutex);
            file_index = std::move(files);
            dir_index = std::move(dirs);
        }

        indexing = false;

        SaveToFile(path);  // Save after indexing
        return {dir_index, file_index};
    }

    


    void Shutdown() {
        indexing = false;
        if (index_thread.joinable()) {
            index_thread.join();
        }
    }
}

