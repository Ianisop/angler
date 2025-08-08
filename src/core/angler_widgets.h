#pragma once
#include <string>
#include <glad/glad.h> 
#include "icons.h"


struct Tab {
    std::string name;
    std::string path;
    Icons::ICON_TYPE icon_type;
    
};