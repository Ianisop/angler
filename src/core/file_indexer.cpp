#include "file_indexer.h"
#include <iostream>
#include <fstream>
#include <algorithm>
#include <cctype>
#include <zstd.h>
#include <future>
#include <unordered_set>

using json = nlohmann::json;

#define COMPRESSION_LEVEL 1

// ---------------- IndexedFile JSON ----------------

void IndexedFile::to_json(json& j) const {
    j = json{
        {"name", name},
        {"path", path},
        {"size", size},
        {"is_directory", is_directory},
        {"extension", extension},
        {"last_modified", std::chrono::duration_cast<std::chrono::seconds>(
            last_modified.time_since_epoch()).count()}
    };
}

void IndexedFile::from_json(const json& j) {
    name = j.at("name").get<std::string>();
    path = j.at("path").get<std::string>();
    size = j.at("size").get<std::uintmax_t>();
    is_directory = j.value("is_directory", false);
    extension = j.value("extension", "");
    auto secs = j.at("last_modified").get<std::uint64_t>();
    last_modified = std::filesystem::file_time_type(std::chrono::seconds(secs));
}

// ---------------- FileIndexer Namespace ----------------

namespace FileIndexer {
    namespace {
        std::vector<IndexedFile> index;
        std::thread index_thread;
        std::atomic<bool> indexing{false};
        std::mutex index_mutex;
    }

    void IndexDirectoryNonRecursive(const std::filesystem::path& directory, std::vector<IndexedFile>& out, std::vector<std::filesystem::path>& subdirs) {
        try {
            for (const auto& entry : std::filesystem::directory_iterator(directory)) {
                if (!indexing) break;

                IndexedFile file;
                file.name = entry.path().filename().string();
                file.path = entry.path().string();
                file.last_modified = entry.last_write_time();

                if (entry.is_directory()) {
                    file.is_directory = true;
                    file.size = 0;
                    file.extension = "";
                    subdirs.push_back(entry.path());
                } else if (entry.is_regular_file()) {
                    file.is_directory = false;
                    file.size = entry.file_size();
                    file.extension = entry.path().extension().string();
                } else {
                    continue;
                }

                out.push_back(std::move(file));
            }
        } catch (const std::exception& e) {
            std::cerr << "Directory index error: " << e.what() << "\n";
        }
    }

    void IndexRecursiveAsync(const std::filesystem::path& directory) {
        std::vector<IndexedFile> temp_index;
        std::vector<std::filesystem::path> subdirs;

        IndexDirectoryNonRecursive(directory, temp_index, subdirs);

        // Lock and add top-level files and folders
        {
            std::lock_guard<std::mutex> lock(index_mutex);
            index.insert(index.end(), temp_index.begin(), temp_index.end());
        }

        // Launch recursive indexing for subdirectories
        std::vector<std::future<void>> futures;
        for (const auto& subdir : subdirs) {
            futures.push_back(std::async(std::launch::async, [subdir]() {
                std::vector<IndexedFile> sub_index;
                std::vector<std::filesystem::path> inner_subdirs;
                IndexDirectoryNonRecursive(subdir, sub_index, inner_subdirs);

                {
                    std::lock_guard<std::mutex> lock(index_mutex);
                    index.insert(index.end(), sub_index.begin(), sub_index.end());
                }

                // Optionally: recurse further (not done here to avoid deep nesting)
                // You could queue inner_subdirs back into a thread pool here
            }));
        }

        for (auto& f : futures) {
            if (!indexing) break;
            f.get();
        }

        indexing = false;
    }

    void StartIndexing(const std::string& directory) {
        if (indexing) return;

        indexing = true;

        if (index_thread.joinable())
            index_thread.join();

        index_thread = std::thread([directory]() {
            IndexRecursiveAsync(directory);
        });
    }

    bool IsIndexing() {
        return indexing;
    }

    const std::vector<IndexedFile>& GetIndex() {
        return index;
    }

    std::vector<IndexedFile> Search(const std::string& query) {
        std::vector<IndexedFile> results;
        std::lock_guard<std::mutex> lock(index_mutex);

        auto to_lower = [](const std::string& s) {
            std::string lower;
            std::transform(s.begin(), s.end(), std::back_inserter(lower),
                           [](unsigned char c) { return std::tolower(c); });
            return lower;
        };

        std::string lower_query = to_lower(query);

        for (const auto& file : index) {
            if (to_lower(file.name).find(lower_query) != std::string::npos) {
                results.push_back(file);
            }
        }

        return results;
    }

    void SaveToFile(const std::string& path) {
        std::lock_guard<std::mutex> lock(index_mutex);
        json j = json::array();
    
        for (const auto& file : index) {
            json f;
            file.to_json(f);
            j.push_back(f);
        }
    
        // Write raw JSON file first
        std::string fullPath = path + "/." + "index";
        std::ofstream out(fullPath);
        if (out.is_open()) {
            out << j.dump(2);
            out.close();
            std::cout << "Saved Index to " << fullPath << std::endl;
        } else {
            std::cerr << "Failed to save index to " << path<< "\n";
            return;
        }
    
        // ---------- ZSTD COMPRESSION ----------
        // Read JSON file into memory
        std::ifstream inFile(fullPath, std::ios::binary | std::ios::ate);
        if (!inFile) {
            std::cerr << "Failed to open file for compression: " << fullPath << std::endl;
            return;
        }
    
        std::streamsize inputSize = inFile.tellg();
        inFile.seekg(0, std::ios::beg);
    
        std::vector<char> inputBuffer(inputSize);
        if (!inFile.read(inputBuffer.data(), inputSize)) {
            std::cerr << "Failed to read file into buffer for compression\n";
            return;
        }
    
        // Allocate buffer for compressed data
        size_t bound = ZSTD_compressBound(inputSize);
        std::vector<char> compressed(bound);
    
        // Compress
        size_t compSize = ZSTD_compress(
            compressed.data(), bound,
            inputBuffer.data(), inputSize,
            COMPRESSION_LEVEL 
        );
    
        if (ZSTD_isError(compSize)) {
            std::cerr << "Compression error: " << ZSTD_getErrorName(compSize) << "\n";
            return;
        }
    
        // Write compressed data to disk
        std::string compressedFilePath = fullPath + ".zst";
        std::ofstream compressedFile(compressedFilePath, std::ios::binary);
        if (!compressedFile) {
            std::cerr << "Failed to open file for writing compressed data\n";
            return;
        }
    
        compressedFile.write(compressed.data(), compSize);
        compressedFile.close();
    
        std::cout << "Compressed index written to " << compressedFilePath << std::endl;
    }
    

    bool LoadFromFile(const std::string& path) {
        std::string compressedFilePath = path + "/." + "index" + ".zst";
    
        // Read compressed file
        std::ifstream in(compressedFilePath, std::ios::binary | std::ios::ate);
        if (!in.is_open()) {
            std::cerr << "Failed to load compressed index from " << compressedFilePath << "\n";
            return false;
        }
    
        std::streamsize compSize = in.tellg();
        in.seekg(0, std::ios::beg);
    
        std::vector<char> compressedBuffer(compSize);
        if (!in.read(compressedBuffer.data(), compSize)) {
            std::cerr << "Failed to read compressed index file\n";
            return false;
        }
    
        // Get decompressed size
        unsigned long long decompressedSize = ZSTD_getFrameContentSize(compressedBuffer.data(), compSize);
        if (decompressedSize == ZSTD_CONTENTSIZE_ERROR || decompressedSize == ZSTD_CONTENTSIZE_UNKNOWN) {
            std::cerr << "Invalid or unknown compressed index size\n";
            return false;
        }
    
        std::vector<char> decompressedBuffer(decompressedSize);
    
        size_t actualDecompressedSize = ZSTD_decompress(
            decompressedBuffer.data(), decompressedSize,
            compressedBuffer.data(), compSize
        );
    
        if (ZSTD_isError(actualDecompressedSize)) {
            std::cerr << "Decompression error: " << ZSTD_getErrorName(actualDecompressedSize) << "\n";
            return false;
        }
    
        // Parse JSON from decompressed buffer
        json j;
        try {
            j = json::parse(decompressedBuffer.data(), decompressedBuffer.data() + actualDecompressedSize);
        } catch (const std::exception& e) {
            std::cerr << "JSON parsing error: " << e.what() << "\n";
            return false;
        }
    
        // Convert JSON to vector<IndexedFile>
        std::vector<IndexedFile> temp;
        for (const auto& item : j) {
            IndexedFile file;
            file.from_json(item);
            temp.push_back(file);
        }
    
        {
            std::lock_guard<std::mutex> lock(index_mutex);
            index = std::move(temp);
        }
    
        std::cout << "Loaded and decompressed index from " << compressedFilePath << "\n";
        return true;
    }
    
    std::vector<IndexedFile> ShowFilesInTab(const std::string& path) {
        if (LoadFromFile(path)) {
            std::cout << "found index, loading" << std::endl;
            return FileIndexer::Search("");
        } else {
            std::thread indexing_thread([path]() {
                std::cout << "Searching in: " << path << std::endl;
                FileIndexer::StartIndexing(path);
                while (FileIndexer::IsIndexing()) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                }
                FileIndexer::SaveToFile(path);  
                return FileIndexer::Search("");
            });
            indexing_thread.detach();
    
            
            return {};
        }
    }
    
    
    
    
    void Shutdown() {
        if (index_thread.joinable()) {
            indexing = false;
            index_thread.join();
        }
    }
}
