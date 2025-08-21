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

void fileindexer::IndexedFile::to_json(json &j) const
{
    j = json{
        {"name", name},
        {"path", path},
        {"size", size},
        {"extension", extension},
        {"last_modified", std::chrono::duration_cast<std::chrono::seconds>(
                              last_modified.time_since_epoch())
                              .count()}};
}

void fileindexer::IndexedFile::from_json(const json &j)
{
    name = j.at("name").get<std::string>();
    path = j.at("path").get<std::string>();
    size = j.at("size").get<std::uintmax_t>();
    extension = j.value("extension", "");
    auto secs = j.at("last_modified").get<uint64_t>();
    last_modified = std::filesystem::file_time_type(std::chrono::seconds(secs));
}

void fileindexer::IndexedDirectory::to_json(json &j) const
{
    j = json{
        {"name", name},
        {"path", path},
        {"size", size},
        {"last_modified", std::chrono::duration_cast<std::chrono::seconds>(
                              last_modified.time_since_epoch())
                              .count()}};
}

void fileindexer::IndexedDirectory::from_json(const json &j)
{
    name = j.at("name").get<std::string>();
    path = j.at("path").get<std::string>();
    size = j.at("size").get<std::uintmax_t>();
    auto secs = j.at("last_modified").get<uint64_t>();
    last_modified = std::filesystem::file_time_type(std::chrono::seconds(secs));
}

// small RAII guard to always reset the indexing flag
struct IndexingGuard
{
    std::atomic<bool> &flag;
    explicit IndexingGuard(std::atomic<bool> &f) : flag(f)
    {
        flag = true;
    }
    ~IndexingGuard()
    {
        flag = false;
    }
};

// ---------------- FileIndexer Implementation ----------------

namespace fileindexer
{
    namespace
    {
        std::unordered_map<std::filesystem::path, IndexedFile> file_index;
        std::unordered_map<std::filesystem::path, IndexedDirectory> dir_index;

        std::thread index_thread;
        std::atomic<bool> indexing{false};
        std::mutex index_mutex;
    }


    std::uintmax_t GetDirectorySize(const std::filesystem::path &dir)
    {

        std::uintmax_t total_size = 0;
        try
        {
            for (const auto &entry : std::filesystem::recursive_directory_iterator(
                     dir, std::filesystem::directory_options::skip_permission_denied))
            {
                if (!indexing) break;
                if (!entry.is_regular_file()) continue;
                std::error_code ec;
                total_size += entry.file_size(ec);
            }
        }
        catch (const std::exception &e)
        {
            std::cerr << "Error calculating size for " << dir << ": " << e.what() << '\n';
        }
        return total_size;
    }

    long GetFileSize(std::string filename)
    {
        struct stat stat_buf;
        int rc = stat(filename.c_str(), &stat_buf);
        return rc == 0 ? stat_buf.st_size : -1;
    }

    std::string HumanReadableSize(std::uintmax_t size)
    {
        const char *suffixes[] = {"B", "KB", "MB", "GB", "TB"};
        int i = 0;
        double dblSize = static_cast<double>(size);

        while (dblSize >= 1024 && i < 4)
        {
            dblSize /= 1024;
            ++i;
        }

        char buf[64];
        std::snprintf(buf, sizeof(buf), "%.2f %s", dblSize, suffixes[i]);
        return std::string(buf);
    }

    void IndexDirectory(
        const std::filesystem::path &directory,
        std::unordered_map<std::filesystem::path, IndexedFile> &files_out,
        std::unordered_map<std::filesystem::path, IndexedDirectory> &dirs_out,
        std::unordered_map<std::filesystem::path, IndexedFile> &files_from_disk,
        std::unordered_map<std::filesystem::path, IndexedDirectory> &dirs_from_disk)
    {
        MEASURE_TIME("IndexDirectory");
        files_out.clear();
        dirs_out.clear();

        std::error_code ec;
        std::filesystem::directory_iterator it(
            directory,
            std::filesystem::directory_options::skip_permission_denied,
            ec);

        if (ec)
        {
            std::cerr << "Error opening directory: " << directory << " - " << ec.message() << "\n";
            return;
        }

        for (const auto &entry : it)
        {
            if (!indexing) return;

            const auto &path = entry.path();
            ec.clear();
            if (entry.is_directory(ec) && !ec)
            {
                // if the dir has been cached before
                if(dirs_from_disk.find(entry.path()) != dirs_from_disk.end())
                {
                    IndexedDirectory& dir = dirs_from_disk[entry.path()];

                    //if its not been written to since
                    if (entry.last_write_time() <= dir.last_modified)
                    {
                        std::cout << "already indexed: " << dir.name << std::endl;
                        if (!ec) dirs_out[path] = dir;  // copy from the cache
                        continue;
                    }

                    //updated
                    else{
                        dir.name = path.filename().string();
                        dir.path = path;
                        dir.size = GetDirectorySize(path);
                        dir.last_modified = std::filesystem::last_write_time(path, ec);
                    }
                    if (!ec) dirs_out[path] = std::move(dir);
                   
                }
                //not cached
                else {
                    IndexedDirectory dir;
                    dir.name = path.filename().string();
                    dir.path = path;
                    dir.size = GetDirectorySize(path);
                    dir.last_modified = std::filesystem::last_write_time(path, ec);
                    if (!ec) {
                        dirs_out[path] = dir;
                        dirs_from_disk[path] = dir;  // add to cache
                    }
                }
                
            }
            else if (entry.is_regular_file(ec) && !ec)
            {
                IndexedFile file;
                file.name = path.filename().string();
                file.path = path;
                file.size = std::filesystem::file_size(path, ec);
                file.extension = path.extension().string();
                file.extension_type = GetExtensionType(path);
                file.last_modified = std::filesystem::last_write_time(path, ec);
                if (!ec) files_out[path] = std::move(file);
            }
        
        }
    }   

    EXTENSION_TYPE GetExtensionType(std::filesystem::path extension)
    {
        static const std::unordered_map<std::string, EXTENSION_TYPE> mapping = {
            {".txt",EXTENSION_TYPE::TEXT},
            {".doc",EXTENSION_TYPE::DOC},
            {".docx",EXTENSION_TYPE::DOC},  
            {".pdf",EXTENSION_TYPE::PDF},
            {".png", EXTENSION_TYPE::IMAGE},
            {".jpg", EXTENSION_TYPE::IMAGE},
            {".bmp", EXTENSION_TYPE::IMAGE},
            {".mp3", EXTENSION_TYPE::AUDIO},
            {".wav", EXTENSION_TYPE::AUDIO},
            {".flac", EXTENSION_TYPE::AUDIO},
            {".ogg", EXTENSION_TYPE::AUDIO},
            {".mp4", EXTENSION_TYPE::VIDEO},
            {".mkv", EXTENSION_TYPE::VIDEO},
            {".mov", EXTENSION_TYPE::VIDEO},
            {".avi", EXTENSION_TYPE::VIDEO},
            {".hpp", EXTENSION_TYPE::TEXT}, //TODO: FIND ICON FOR THIS
            {".gz", EXTENSION_TYPE::ARCHIVE},
            {".zip", EXTENSION_TYPE::ARCHIVE},
            {".ttf", EXTENSION_TYPE::TEXT}, //TODO: FIND ICON FOR THIS
            {".zst", EXTENSION_TYPE::ARCHIVE}

        };
        

        auto ext = extension.extension().string(); 
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    
        auto it = mapping.find(ext);
        
        return it != mapping.end() ? it->second : EXTENSION_TYPE::FILE;
    }
    

   
    bool IsIndexing()
    {
        return indexing;
    }

    bool LoadFromFile(const std::string &path,
                      std::unordered_map<std::filesystem::path, IndexedDirectory> &dirs_out,
                      std::unordered_map<std::filesystem::path, IndexedFile> &files_out)
    {
        dirs_out.clear();
        files_out.clear();

        const std::string jsonFilePath = path + "/.index";
        std::ifstream in(jsonFilePath, std::ios::binary);

        auto parse_and_fill = [&](const std::string &content) -> bool
        {
            try
            {
                json j = json::parse(content);
                for (const auto &f : j["files"])
                {
                    IndexedFile file;
                    file.from_json(f);
                    files_out[file.path] = std::move(file);
                }
                for (const auto &d : j["dirs"])
                {
                    IndexedDirectory dir;
                    dir.from_json(d);
                    dirs_out[dir.path] = std::move(dir);
                }
                return true;
            }
            catch (const std::exception &e)
            {
                std::cerr << "Failed to parse JSON: " << e.what() << "\n";
                return false;
            }
        };

        if (in.is_open())
        {
            std::string content((std::istreambuf_iterator<char>(in)), {});
            return parse_and_fill(content);
        }

        std::ifstream zstIn(jsonFilePath + ".zst", std::ios::binary | std::ios::ate);
        if (!zstIn.is_open())
        {
            std::cerr << "No index file found at " << jsonFilePath << " (plain or .zst).\n";
            return false;
        }

        size_t size = (size_t)zstIn.tellg();
        zstIn.seekg(0);
        std::vector<char> compressed(size);
        zstIn.read(compressed.data(), size);

        unsigned long long const rSize = ZSTD_getFrameContentSize(compressed.data(), size);
        if (rSize == ZSTD_CONTENTSIZE_ERROR || rSize == ZSTD_CONTENTSIZE_UNKNOWN)
        {
            std::cerr << "Invalid or unknown compressed content size\n";
            return false;
        }

        std::vector<char> decompressed(rSize);
        size_t dSize = ZSTD_decompress(decompressed.data(), rSize, compressed.data(), size);
        if (ZSTD_isError(dSize))
        {
            std::cerr << "Decompression failed: " << ZSTD_getErrorName(dSize) << "\n";
            return false;
        }

        return parse_and_fill(std::string(decompressed.data(), decompressed.data() + dSize));
    }

    std::vector<IndexedFile> Search(const std::string &query, bool include_dirs)
    {
        std::vector<IndexedFile> results;
        std::lock_guard<std::mutex> lock(index_mutex);

        auto to_lower = [](const std::string &s)
        {
            std::string lower;
            lower.reserve(s.size());
            std::transform(s.begin(), s.end(), std::back_inserter(lower),
                           [](unsigned char c)
                           { return static_cast<char>(std::tolower(c)); });
            return lower;
        };

        std::string lower_query = to_lower(query);

        for (const auto &[_, file] : file_index)
        {
            if (to_lower(file.name).find(lower_query) != std::string::npos)
            {
                results.push_back(file);
            }
        }

        if (include_dirs)
        {
            for (const auto &[_, dir] : dir_index)
            {
                if (to_lower(dir.name).find(lower_query) != std::string::npos)
                {
                    IndexedFile f;
                    f.name = dir.name;
                    f.path = dir.path;
                    f.size = dir.size;
                    f.extension = "";
                    f.last_modified = dir.last_modified;
                    results.push_back(std::move(f));
                }
            }
        }
        return results;
    }

    void SaveToFile(const std::string &path)
    {
        std::lock_guard<std::mutex> lock(index_mutex);

        if (file_index.empty() && dir_index.empty())
        {
            std::cerr << "SaveToFile: Skipping save because index is empty.\n";
            return;
        }

        json j;
        j["files"] = json::array();
        j["dirs"] = json::array();

        for (const auto &[_, file] : file_index)
        {
            json f;
            file.to_json(f);
            j["files"].push_back(f);
        }
        for (const auto &[_, dir] : dir_index)
        {
            json d;
            dir.to_json(d);
            j["dirs"].push_back(d);
        }

        const std::string base = path + "/.index";
        const std::string tmp = base + ".tmp";

        {
            std::ofstream out(tmp, std::ios::binary);
            if (!out)
            {
                std::cerr << "Failed to save index to " << tmp << "\n";
                return;
            }
            const std::string jsonStr = j.dump(2);
            out.write(jsonStr.data(), jsonStr.size());
        }

        std::error_code ec;
        std::filesystem::rename(tmp, base, ec);
        if (ec)
        {
            std::filesystem::remove(base, ec);
            std::filesystem::rename(tmp, base, ec);
            if (ec)
            {
                std::cerr << "Failed to finalize index file: " << ec.message() << "\n";
                return;
            }
        }

        std::ifstream inFile(base, std::ios::binary | std::ios::ate);
        if (!inFile)
        {
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
        if (ZSTD_isError(compSize))
        {
            std::cerr << "Compression error: " << ZSTD_getErrorName(compSize) << "\n";
            return;
        }

        const std::string zst = base + ".zst";
        const std::string zst_tmp = zst + ".tmp";
        {
            std::ofstream outFile(zst_tmp, std::ios::binary);
            outFile.write(compressed.data(), compSize);
        }
        std::filesystem::rename(zst_tmp, zst, ec);
        if (ec)
        {
            std::filesystem::remove(zst, ec);
            std::filesystem::rename(zst_tmp, zst, ec);
        }

        if (std::remove(base.c_str()) != 0)
        {
            std::cerr << "Failed to delete temporary .index file\n";
        }
    }

    std::tuple<std::unordered_map<std::filesystem::path, IndexedDirectory>,
               std::unordered_map<std::filesystem::path, IndexedFile>>
    ShowFilesAndDirsContinous(const std::string &path)
    {
        IndexingGuard guard(indexing);

        std::unordered_map<std::filesystem::path, IndexedFile> files_from_disk;
        std::unordered_map<std::filesystem::path, IndexedDirectory> dirs_from_disk;
        dirs_from_disk.clear();
        files_from_disk.clear();
        LoadFromFile(path, dirs_from_disk, files_from_disk);

        std::unordered_map<std::filesystem::path, IndexedFile> files;
        std::unordered_map<std::filesystem::path, IndexedDirectory> dirs;

        IndexDirectory(path, files, dirs,files_from_disk,dirs_from_disk);

        {
            std::lock_guard<std::mutex> lock(index_mutex);
            file_index = std::move(files);
            dir_index = std::move(dirs);
        }

        SaveToFile(path);

        return {dir_index, file_index};
    }

    void Shutdown()
    {
        indexing = false;
        if (index_thread.joinable())
            index_thread.join();
    }

    void StartIndexing(const std::string &directory)
    {
        if (indexing) return;

        if (index_thread.joinable())
            index_thread.join();

        {
            std::lock_guard<std::mutex> lock(index_mutex);
            file_index.clear();
            dir_index.clear();
        }

        index_thread = std::thread([directory]()
        {
            
        });
    }
}
