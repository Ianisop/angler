#pragma once

#include <string>
#include <fstream>
#include <vector>
#include <sstream>
#include <iostream>
#include "angler_widgets.h"
#include <unordered_set>

// Declare externally defined tab vector
extern std::vector<Tab> tabs;

namespace AnglerFileIO 
{

    // Load tab data from a .angler file
    inline bool LoadTabsFromFile(const std::string& filename) 
    {
        std::ifstream infile(filename);
        if (!infile.is_open()) 
        {
            std::cerr << "[Angler] Failed to open file: " << filename << "\n";
            return false;
        }
    
        tabs.clear(); // Clear existing tabs
    
        std::string line;
        while (std::getline(infile, line)) 
        {
            size_t separator = line.find('|');
            if (separator == std::string::npos) 
            {
                std::cerr << "Invalid line in .angler file: " << line << std::endl;
                continue;
            }
        
            std::string name = line.substr(0, separator);
            std::string path = line.substr(separator + 1);
        
            tabs.push_back(Tab(name, path));
        }
        
    
        return true;
    }
    

    inline bool SaveTabsToFile(const std::string& filename) 
    {
        std::unordered_set<std::string> existingTabs;
    
        // Load existing tabs from file
        std::ifstream infile(filename);
        if (infile.is_open()) 
        {
            std::string line;
            while (std::getline(infile, line)) 
            {
                existingTabs.insert(line); // store each full "name|path" line
            }
            infile.close();
        }
    
        // Open file for appending
        std::ofstream outfile(filename, std::ios::app);
        if (!outfile.is_open()) 
        {
            std::cerr << "[Angler] Failed to open file for writing: " << filename << "\n";
            return false;
        }
    
        // Write only tabs that aren't already in the file
        for (const auto& tab : tabs)
        {
            std::string line = tab.name + "|" + tab.path.string();
            if (existingTabs.find(line) == existingTabs.end())
            {
                outfile << line << "\n";
            }
        }
    
        return true;
    }
    
    inline bool RemoveTabFromFile(const std::string& filename, const std::string& tabPath)
    {
        //std::cout << "REMOVING FROM PATH: " << tabPath << std::endl;
        std::ifstream infile(filename);
        if (!infile.is_open())
        {
            std::cerr << "[Angler] Failed to open file for reading: " << filename << "\n";
            return false;
        }
    
        // Read all lines into memory
        std::vector<std::string> lines;
        std::string line;
        while (std::getline(infile, line)) 
        {
            // Each line format: name|path
            auto sep = line.find('|');
            if (sep != std::string::npos) 
            {
                std::string path = line.substr(sep + 1);
                if (path == tabPath) continue; // skip this tab
            }
            lines.push_back(line);
        }
        infile.close();
    
        // Rewrite file without the removed tab
        std::ofstream outfile(filename, std::ios::trunc);
        if (!outfile.is_open()) 
        {
            std::cerr << "[Angler] Failed to open file for writing: " << filename << "\n";
            return false;
        }
    
        for (const auto& l : lines) 
        {
            //std::cout << l << std::endl;
            outfile << l << "\n";
        }
    
        return true;
    }
    
}
