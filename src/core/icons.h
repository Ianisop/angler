#pragma once
#include <cstdio>
#include <iostream>
#include <string>
#include <unordered_map>
#include "core/file_indexer.h"
#include "texture_loader.cpp"

namespace Icons 
{
    
    int ICON_SIZE_SMALL = 16;
    int ICON_SIZE_MEDIUM = 32;
    int ICON_SIZE_BIG = 64;
    


    struct IconData 
    {
        int size;
        fileindexer::EXTENSION_TYPE extension_type;
    
        IconData(int icon_size, fileindexer::EXTENSION_TYPE extension)
            : size(icon_size), extension_type(extension) {}
    
        bool operator==(const IconData& other) const 
        {
            return size == other.size && extension_type == other.extension_type;
        }
        
    };
}

namespace std 
{
    template<>
    struct hash<Icons::IconData> 
    {
        std::size_t operator()(const Icons::IconData& k) const 
        {
            return std::hash<int>()(k.extension_type) ^ (std::hash<int>()(k.size) << 1);
        }
    };
}

namespace Icons {
    
    // Icon path map
    static std::unordered_map<IconData, std::string> icons = 
    {
        { { ICON_SIZE_SMALL, fileindexer::FILE },  "src/core/assets/icons/16/file.png" },
        { { ICON_SIZE_MEDIUM,fileindexer::FILE  }, "src/core/assets/icons/32/file.png" },
        { { ICON_SIZE_BIG,fileindexer::FILE  },    "src/core/assets/icons/64/file.png" },

        { { ICON_SIZE_SMALL,fileindexer::DIRECTORY  },   "src/core/assets/icons/16/folder.png" },
        { { ICON_SIZE_MEDIUM,fileindexer::DIRECTORY  },  "src/core/assets/icons/32/folder.png" },
        { { ICON_SIZE_BIG,fileindexer::DIRECTORY  },     "src/core/assets/icons/64/folder.png" },

        { { ICON_SIZE_SMALL, fileindexer::AUDIO },    "src/core/assets/icons/16/audio.png" },
        { { ICON_SIZE_MEDIUM,fileindexer::AUDIO },   "src/core/assets/icons/32/audio.png" },
        { { ICON_SIZE_BIG,fileindexer::AUDIO },      "src/core/assets/icons/64/audio.png" },

        { { ICON_SIZE_SMALL,fileindexer::IMAGE },  "src/core/assets/icons/16/image.png" },
        { { ICON_SIZE_MEDIUM,fileindexer::IMAGE }, "src/core/assets/icons/32/image.png" },
        { { ICON_SIZE_BIG,fileindexer::IMAGE },    "src/core/assets/icons/64/image.png" },
  
        { { ICON_SIZE_SMALL,fileindexer::VIDEO },    "src/core/assets/icons/16/video.png" },
        { { ICON_SIZE_MEDIUM,fileindexer::VIDEO  },   "src/core/assets/icons/32/video.png" },
        { { ICON_SIZE_BIG,fileindexer::VIDEO  },      "src/core/assets/icons/64/video.png" },

        { { ICON_SIZE_SMALL,fileindexer::ARCHIVE },  "src/core/assets/icons/16/archive.png" },
        { { ICON_SIZE_MEDIUM,fileindexer::ARCHIVE }, "src/core/assets/icons/32/archive.png" },
        { { ICON_SIZE_BIG,fileindexer::ARCHIVE },    "src/core/assets/icons/64/archive.png" },


    };

        // Texture cache 
        static std::unordered_map<IconData, GLuint> texture_cache;

        GLuint FetchIconTextureByType(fileindexer::EXTENSION_TYPE extension, int icon_size, int* out_width, int* out_height) 
        {
            IconData key(icon_size, extension);
        
            // Check cache
            if (texture_cache.find(key) != texture_cache.end()) 
            {
                return texture_cache[key];
            }
        
            // Find filepath
            auto it = icons.find(key);
            std::string filepath;

            if (!std::filesystem::exists(filepath))
                std::cerr << "MISSING ICON FILE: " << filepath << "\n";
        
            if (it != icons.end()) 
            {
                filepath = it->second;
            } else 
            {
                filepath = icons[{icon_size, fileindexer::FILE}]; // fallback
            }
        
            // Load and cache
            GLuint texture = LoadTextureFromFile(filepath.c_str(), out_width, out_height);
            texture_cache[key] = texture;
            return texture;
        }
        
}  

