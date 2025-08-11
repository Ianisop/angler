#pragma once

#include <string>
#include <vector>
#include <filesystem>
#include <mutex>
#include <thread>
#include <atomic>
#include "json.hpp"

using json = nlohmann::json;

struct IndexedFile {
    std::string name;
    std::string path;
    std::uintmax_t size;
    std::filesystem::file_time_type last_modified;

    void to_json(nlohmann::json& j) const;
    void from_json(const nlohmann::json& j);
};

namespace FileIndexer {
    void StartIndexing(const std::string& directory);
    bool LoadFromFile(const std::string& path);
    void SaveToFile(const std::string& path);
    std::vector<IndexedFile> ShowFilesInTab(const std::string& path);
    std::vector<IndexedFile> Search(const std::string& query);
    bool IsIndexing();
    const std::vector<IndexedFile>& GetIndex();
    void Shutdown(); 
}
