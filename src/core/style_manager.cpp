#include <cstdint>
#include <iostream>
#include <fstream>
#include <unordered_map>
#include <imgui.h>

std::unordered_map<std::string, int> colorNameToEnum = {
    {"WindowBg", ImGuiCol_WindowBg},
    {"Button", ImGuiCol_Button},
    {"ButtonHovered", ImGuiCol_ButtonHovered},
    {"ButtonActive", ImGuiCol_ButtonActive},
    {"Tab", ImGuiCol_Tab}
};



#include <fstream>
#include <cstring>

bool LoadStyleFromScale(const char* filename)
{
    ImGuiStyle& style = ImGui::GetStyle();
    std::ifstream ifs(filename, std::ios::binary);
    if (!ifs.is_open()) {
        printf("Failed to open file %s\n", filename);
        return false;
    }

    char magic[5] = {0};
    ifs.read(magic, 4);
    printf("Magic: %s\n", magic);
    if (std::strncmp(magic, "IMGS", 4) != 0) {
        printf("Invalid file header\n");
        return false;
    }

    uint32_t version = 0;
    ifs.read(reinterpret_cast<char*>(&version), sizeof(version));
    printf("Version: %u\n", version);
    if (version != 1) {
        printf("Unsupported version\n");
        return false;
    }

    uint32_t count = 0;
    ifs.read(reinterpret_cast<char*>(&count), sizeof(count));
    printf("Color count: %u\n", count);
    if (count != ImGuiCol_COUNT) {
        printf("Color count mismatch\n");
        return false;
    }

    for (uint32_t i = 0; i < count; ++i) {
        ImVec4 c;
        ifs.read(reinterpret_cast<char*>(&c.x), sizeof(float));
        ifs.read(reinterpret_cast<char*>(&c.y), sizeof(float));
        ifs.read(reinterpret_cast<char*>(&c.z), sizeof(float));
        ifs.read(reinterpret_cast<char*>(&c.w), sizeof(float));
        if (ifs.fail()) {
            printf("Failed to read color %u\n", i);
            return false;
        }
        style.Colors[i] = c;
    }

    ifs.close();
    printf("Style loaded successfully\n");
    return true;
}



void SaveStyleToScale(const char* filename)
{
    ImGuiStyle& style = ImGui::GetStyle();
    std::ofstream ofs(filename, std::ios::binary);

    // Write magic header "IMGS"
    ofs.write("IMGS", 4);

    uint32_t version = 1;
    ofs.write(reinterpret_cast<char*>(&version), sizeof(version));

    uint32_t count = ImGuiCol_COUNT;
    ofs.write(reinterpret_cast<char*>(&count), sizeof(count));

    for (uint32_t i = 0; i < count; ++i) {
        const ImVec4& c = style.Colors[i];
        ofs.write(reinterpret_cast<const char*>(&c.x), sizeof(float));
        ofs.write(reinterpret_cast<const char*>(&c.y), sizeof(float));
        ofs.write(reinterpret_cast<const char*>(&c.z), sizeof(float));
        ofs.write(reinterpret_cast<const char*>(&c.w), sizeof(float));
    }
    ofs.close();
}

