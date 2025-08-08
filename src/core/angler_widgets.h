#pragma once
#include <string>
#include <glad/glad.h> 
#include "icons.h"


struct Tab {
    std::string name;
    std::string path;
    Tab(std::string tab_name, std::string tab_path)
        : name(tab_name), path(tab_path) {}
    
};