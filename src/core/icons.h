#pragma once
#include <string>
#include <unordered_map>
#include "texture_loader.cpp"

namespace Icons 
{
    
    int ICON_SIZE_SMALL = 16;
    int ICON_SIZE_MEDIUM = 32;
    int ICON_SIZE_BIG = 64;
    


    enum ICON_TYPE 
    {
        DEFAULT,
        FOLDER,
        AUDIO,
        PICTURE,
        VIDEO,
        TARBALL,
        COMPRESSED
    };
    
    struct IconData 
    {
        ICON_TYPE type;
        int size;
    
        IconData(ICON_TYPE icon_type, int icon_size)
            : type(icon_type), size(icon_size) {}
    
        bool operator==(const IconData& other) const 
        {
            return type == other.type && size == other.size;
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
            return std::hash<int>()(k.type) ^ (std::hash<int>()(k.size) << 1);
        }
    };
}

namespace Icons {
    // Icon path map
    static std::unordered_map<IconData, std::string> icons = 
    {
        { { DEFAULT, ICON_SIZE_SMALL },  "src/core/assets/icons/16/file.png" },
        { { DEFAULT, ICON_SIZE_MEDIUM }, "src/core/assets/icons/32/file.png" },
        { { DEFAULT, ICON_SIZE_BIG },    "src/core/assets/icons/64/file.png" },

        { { FOLDER, ICON_SIZE_SMALL },   "src/core/assets/icons/16/folder.png" },
        { { FOLDER, ICON_SIZE_MEDIUM },  "src/core/assets/icons/32/folder.png" },
        { { FOLDER, ICON_SIZE_BIG },     "src/core/assets/icons/64/folder.png" },

        { { AUDIO, ICON_SIZE_SMALL },    "src/core/assets/icons/16/audio.png" },
        { { AUDIO, ICON_SIZE_MEDIUM },   "src/core/assets/icons/32/audio.png" },
        { { AUDIO, ICON_SIZE_BIG },      "src/core/assets/icons/64/audio.png" },

        { { PICTURE, ICON_SIZE_SMALL },  "src/core/assets/icons/16/picture.png" },
        { { PICTURE, ICON_SIZE_MEDIUM }, "src/core/assets/icons/32/picture.png" },
        { { PICTURE, ICON_SIZE_BIG },    "src/core/assets/icons/64/picture.png" },

        { { VIDEO, ICON_SIZE_SMALL },    "src/core/assets/icons/16/video.png" },
        { { VIDEO, ICON_SIZE_MEDIUM },   "src/core/assets/icons/32/video.png" },
        { { VIDEO, ICON_SIZE_BIG },      "src/core/assets/icons/64/video.png" },

        { { TARBALL, ICON_SIZE_SMALL },  "src/core/assets/icons/16/tarball.png" },
        { { TARBALL, ICON_SIZE_MEDIUM }, "src/core/assets/icons/32/tarball.png" },
        { { TARBALL, ICON_SIZE_BIG },    "src/core/assets/icons/64/tarball.png" },

        { { COMPRESSED, ICON_SIZE_SMALL },  "src/core/assets/icons/16/compressed.png" },
        { { COMPRESSED, ICON_SIZE_MEDIUM }, "src/core/assets/icons/32/compressed.png" },
        { { COMPRESSED, ICON_SIZE_BIG },    "src/core/assets/icons/64/compressed.png" }
    };

        

        // Texture cache 
        static std::unordered_map<IconData, GLuint> texture_cache;

        GLuint FetchIconTextureByType(ICON_TYPE icon_type, int icon_size, int* out_width, int* out_height) 
        {
            IconData key(icon_type, icon_size);
        
            // Check cache
            if (texture_cache.find(key) != texture_cache.end()) 
            {
                return texture_cache[key];
            }
        
            // Find filepath
            auto it = icons.find(key);
            std::string filepath;
        
            if (it != icons.end()) 
            {
                filepath = it->second;
            } else 
            {
                filepath = icons[{ DEFAULT, icon_size }]; // fallback
            }
        
            // Load and cache
            GLuint texture = LoadTextureFromFile(filepath.c_str(), out_width, out_height);
            texture_cache[key] = texture;
            return texture;
        }
        
}  

