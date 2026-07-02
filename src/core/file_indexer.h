#pragma once

#include <string>
#include <vector>
#include <filesystem>
#include <mutex>
#include <thread>
#include <atomic>
#include <unordered_map>
#include <tuple>
#include "json.hpp"

using json = nlohmann::json;

namespace fileindexer {

    enum EXTENSION_TYPE
    {
        DIRECTORY,
        ARCHIVE,
        FILE,
        AUDIO,
        VIDEO,
        IMAGE,
        TEXT,
        PDF,
        DOC,
        PPT,
        SPREADSHEET
    };

    struct IndexedFile {
        std::string name;
        std::filesystem::path path;
        std::uintmax_t size;
        std::filesystem::file_time_type last_modified;
        EXTENSION_TYPE extension_type;
        std::string extension;
    
        void to_json(json& j) const;
        void from_json(const json& j);
    };
    
    struct IndexedDirectory {
        std::string name;
        std::filesystem::path path;
        std::uintmax_t size;
        std::filesystem::file_time_type last_modified;
    
        void to_json(json& j) const;
        void from_json(const json& j);
    };

    void StartIndexing(const std::string& directory);
    bool LoadFromFile(const std::string& path);
    void SaveToFile(const std::string& path);
    EXTENSION_TYPE GetExtensionType(std::filesystem::path extension);
    std::vector<IndexedFile> ShowFilesInTab(const std::string& path);
    std::vector<IndexedFile> SearchFiles(const std::string& query);
    std::vector<IndexedDirectory> SearchDirectories(const std::string& query);
    std::tuple<std::vector<IndexedDirectory>, std::vector<IndexedFile>> ShowFilesAndDirsInTab(const std::filesystem::path path);
    std::tuple<std::unordered_map<std::filesystem::path, IndexedDirectory>, std::unordered_map<std::filesystem::path, IndexedFile>> ShowFilesAndDirsContinuous(const std::filesystem::path& path);
    std::uintmax_t GetDirectorySize(const std::filesystem::path& dir);
    std::string HumanReadableSize(std::uintmax_t size);
    bool IsIndexing();
    const std::unordered_map<std::filesystem::path, IndexedFile>& GetFileIndex();
    const std::unordered_map<std::filesystem::path, IndexedDirectory>& GetDirectoryIndex();
    void Shutdown(); 
}