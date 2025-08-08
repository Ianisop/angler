#pragma once

#include <string>
#include <fstream>
#include <vector>
#include <sstream>
#include <iostream>

// Declare externally defined tab vector
extern std::vector<Tab> tabs;

namespace AnglerFileIO {

    // Load tab data from a .angler file
    inline bool LoadTabsFromFile(const std::string& filename) {
        std::ifstream infile(filename);
        if (!infile.is_open()) {
            std::cerr << "[Angler] Failed to open file: " << filename << "\n";
            return false;
        }
    
        tabs.clear(); // Clear existing tabs
    
        std::string line;
        while (std::getline(infile, line)) {
            size_t separator = line.find('|');
            if (separator == std::string::npos) {
                std::cerr << "Invalid line in .angler file: " << line << std::endl;
                continue;
            }
        
            std::string name = line.substr(0, separator);
            std::string path = line.substr(separator + 1);
        
            tabs.push_back(Tab(name, path));
        }
        
    
        return true;
    }
    

    // Save current tabs to a .angler file
    inline bool SaveTabsToFile(const std::string& filename) {
        std::ofstream outfile(filename);
        if (!outfile.is_open()) {
            std::cerr << "[Angler] Failed to open file for writing: " << filename << "\n";
            return false;
        }
    
        for (const auto& tab : tabs) {
            outfile << tab.name << "|" << tab.path << "\n";
        }
    
        return true;
    }
    
}
