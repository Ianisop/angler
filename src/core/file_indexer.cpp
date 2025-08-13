#include "file_indexer.h"
#include <iostream>
#include <fstream>
#include <algorithm>
#include <cctype>
#include <zstd.h>
#include <chrono>
#include <sys/stat.h>
#include <unordered_map>
#include <chrono>
#include "scoped_timer.h"



using json = nlohmann::json;

#define COMPRESSION_LEVEL 1
#define DEBUG_MEASURE_TIMES 1 // Set to 1 to enable timing debug messages

#ifdef DEBUG_MEASURE_TIMES 
#define MEASURE_TIME(label) ScopedTimer timer__(label)
#else
#define MEASURE_TIME(label)
#endif


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
        std::unordered_map<std::filesystem::path, IndexedFile> file_index;
        std::unordered_map<std::filesystem::path, IndexedDirectory> dir_index;

        std::thread index_thread;
        std::atomic<bool> indexing{false};
        std::mutex index_mutex;
    }

    void IndexDirectory(
        const std::filesystem::path& directory,
        std::unordered_map<std::filesystem::path, IndexedFile>& files_out,
        std::unordered_map<std::filesystem::path, IndexedDirectory>& dirs_out
    ) {
        MEASURE_TIME("IndexDirectory");

        std::error_code ec;
        std::filesystem::directory_iterator it(directory, std::filesystem::directory_options::skip_permission_denied, ec);

        if (ec) {
            std::cerr << "Error opening directory: " << directory << " - " << ec.message() << "\n";
            return;
        }
        //move current index into temp
        {
            std::lock_guard<std::mutex> lock(index_mutex);
            files_out = file_index; 
            dirs_out = dir_index;   
        }
        for (const auto& entry : it) {
            if (!indexing) return;

            const std::filesystem::path& path = entry.path();
            if (files_out.find(entry.path()) != files_out.end() ||
                dirs_out.find(entry.path()) != dirs_out.end()) {
                continue; // Skip already processed entries in this run
            }


            ec.clear();

            if (entry.is_directory(ec) && !ec) {
                IndexedDirectory dir;
                dir.name = path.filename().string();
                dir.path = path;
                dir.size = GetDirectorySize(path);  // still recursive, possibly slow
                dir.last_modified = std::filesystem::last_write_time(path, ec);

                if (!ec)
                    dirs_out[path] = std::move(dir);

            } else if (entry.is_regular_file(ec) && !ec) {
                IndexedFile file;
                file.name = path.filename().string();
                file.path = path;
                file.size = std::filesystem::file_size(path, ec);
                file.extension = path.extension().string();
                file.last_modified = std::filesystem::last_write_time(path, ec);

                if (!ec)
                    files_out[path] = std::move(file);
            }
        }

        indexing = false;
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
            for (const auto& entry : std::filesystem::recursive_directory_iterator(dir,
                    std::filesystem::directory_options::skip_permission_denied)) {

                if (!entry.is_regular_file()) continue;

                std::error_code ec;
                total_size += entry.file_size(ec); // Non-throwing overload
            }
        } catch (const std::exception& e) {
            std::cerr << "Error calculating size for " << dir << ": " << e.what() << '\n';
        }

        return total_size;
    }



    void IndexAsync(const std::string& root) {
        std::unordered_map<std::filesystem::path, IndexedFile> temp_files;
        std::unordered_map<std::filesystem::path, IndexedDirectory> temp_dirs;

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

                std::unordered_map<std::filesystem::path, IndexedFile> temp_files;
                std::unordered_map<std::filesystem::path, IndexedDirectory> temp_dirs;

                for (const auto& f : j["files"]) {
                    IndexedFile file;
                    file.from_json(f);
                    temp_files[file.path] = file;
                }
                for (const auto& d : j["dirs"]) {
                    IndexedDirectory dir;
                    dir.from_json(d);
                    temp_dirs[dir.path] = dir;
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

            std::unordered_map<std::filesystem::path, IndexedFile> temp_files;
            std::unordered_map<std::filesystem::path, IndexedDirectory> temp_dirs;

            for (const auto& f : j["files"]) {
                IndexedFile file;
                file.from_json(f);
                temp_files[file.path] = file;
            }
            for (const auto& d : j["dirs"]) {
                IndexedDirectory dir;
                dir.from_json(d);
                temp_dirs[dir.path] = dir;
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


    //TODO: make this only index changes that arent present in the .index file
    std::tuple<std::unordered_map<std::filesystem::path, IndexedDirectory>, std::unordered_map<std::filesystem::path, IndexedFile>> ShowFilesAndDirsContinous(const std::string& path) {
        indexing = true;

        LoadFromFile(path); // load from saved .index if exists

        std::unordered_map<std::filesystem::path, IndexedFile> files;
        std::unordered_map<std::filesystem::path, IndexedDirectory> dirs;

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

