#include "file_indexer.h"
#include <iostream>
#include <fstream>
#include <algorithm>
#include <cctype>

using json = nlohmann::json;

// ---------------- IndexedFile JSON ----------------

void IndexedFile::to_json(json& j) const {
    j = json{
        {"name", name},
        {"path", path},
        {"size", size},
        {"last_modified", std::chrono::duration_cast<std::chrono::seconds>(
            last_modified.time_since_epoch()).count()}
    };
}

void IndexedFile::from_json(const json& j) {
    name = j.at("name").get<std::string>();
    path = j.at("path").get<std::string>();
    size = j.at("size").get<std::uintmax_t>();
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

    void IndexRecursive(const std::filesystem::path& directory) {
        std::vector<IndexedFile> temp_index;

        try {
            for (const auto& entry : std::filesystem::recursive_directory_iterator(directory)) {
                if (!indexing) break;

                if (entry.is_regular_file()) {
                    IndexedFile file;
                    file.name = entry.path().filename().string();
                    file.path = entry.path().string();
                    file.size = entry.file_size();
                    file.last_modified = entry.last_write_time();
                    temp_index.push_back(file);
                }
            }
        } catch (const std::exception& e) {
            std::cerr << "Indexing error: " << e.what() << "\n";
        }

        {
            std::lock_guard<std::mutex> lock(index_mutex);
            index = std::move(temp_index);
        }

        indexing = false;
    }

    void StartIndexing(const std::string& directory) {
        if (indexing) return;

        indexing = true;

        if (index_thread.joinable())
            index_thread.join();

        index_thread = std::thread([directory]() {
            IndexRecursive(directory);
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

    void SaveToFile(const std::string& path, const std::string& filename) {
        std::lock_guard<std::mutex> lock(index_mutex);
        json j = json::array();

        for (const auto& file : index) {
            json f;
            file.to_json(f);
            j.push_back(f);
        }

        std::ofstream out(path + "/." + filename);
        if (out.is_open()) {
            out << j.dump(2);
            out.close();
            std::cout << "Saved Index to " << path << "/."<< filename << std::endl;
        } else {
            std::cerr << "Failed to save index to " << filename << "\n";
        }
        
    }

    void LoadFromFile(const std::string& path, const std::string& filename) {
        std::ifstream in(path + "/." + filename);
        if (!in.is_open()) {
            std::cerr << "Failed to load index from " << filename << "\n";
            return;
        }

        json j;
        in >> j;

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
    }

    void Shutdown() {
        if (index_thread.joinable()) {
            indexing = false;
            index_thread.join();
        }
    }
}
