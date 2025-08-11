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
    std::string extension;

    void to_json(json& j) const;
    void from_json(const json& j);
};

struct IndexedDirectory {
    std::string name;
    std::string path;
    std::uintmax_t size;
    std::filesystem::file_time_type last_modified;

    void to_json(json& j) const;
    void from_json(const json& j);
};

namespace FileIndexer {
    void StartIndexing(const std::string& directory);
    bool LoadFromFile(const std::string& path);
    void SaveToFile(const std::string& path);
    std::vector<IndexedFile> ShowFilesInTab(const std::string& path);
    std::vector<IndexedFile> SearchFiles(const std::string& query);
    std::vector<IndexedDirectory> SearchDirectories(const std::string& query);
    std::tuple<std::vector<IndexedDirectory>, std::vector<IndexedFile>> ShowFilesAndDirsInTab(const std::string& path);
    std::uintmax_t GetDirectorySize(const std::filesystem::path& dir);
    std::string HumanReadableSize(std::uintmax_t size);
    bool IsIndexing();
    const std::vector<IndexedFile>& GetFileIndex();
    const std::vector<IndexedDirectory>& GetDirectoryIndex();
    void Shutdown(); 
}
