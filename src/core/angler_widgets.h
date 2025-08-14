#pragma once
#include <string>
#include <glad/glad.h> 
#include "icons.h"
#include <filesystem>


struct Tab {
    std::string name = "";
    std::filesystem::path path;
    Tab(std::string tab_name, std::string tab_path)
        : name(tab_name), path(tab_path) {}
    
};