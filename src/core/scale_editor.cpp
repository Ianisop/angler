#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <cstring>
#include <imgui.h>  
#include <stdint.h>


const char* ImGuiColNames[ImGuiCol_COUNT] = {
    "Text", "TextDisabled", "WindowBg", "ChildBg", "PopupBg", "Border",
    "BorderShadow", "FrameBg", "FrameBgHovered", "FrameBgActive", "TitleBg",
    "TitleBgActive", "TitleBgCollapsed", "MenuBarBg", "ScrollbarBg",
    "ScrollbarGrab", "ScrollbarGrabHovered", "ScrollbarGrabActive",
    "CheckMark", "SliderGrab", "SliderGrabActive", "Button", "ButtonHovered",
    "ButtonActive", "Header", "HeaderHovered", "HeaderActive", "Separator",
    "SeparatorHovered", "SeparatorActive", "ResizeGrip", "ResizeGripHovered",
    "ResizeGripActive", "Tab", "TabHovered", "TabActive", "TabUnfocused",
    "TabUnfocusedActive", "DockingPreview", "DockingEmptyBg", "PlotLines",
    "PlotLinesHovered", "PlotHistogram", "PlotHistogramHovered", "TextSelectedBg",
    "DragDropTarget", "NavHighlight", "NavWindowingHighlight", "NavWindowingDimBg",
    "ModalWindowDimBg"
};

bool LoadStyleFromScale(const char* filename, std::vector<ImVec4>& colors) {
    std::ifstream ifs(filename, std::ios::binary);
    if (!ifs.is_open()) {
        std::cout << "Failed to open file " << filename << "\n";
        return false;
    }

    char magic[4];
    ifs.read(magic, 4);
    if (std::strncmp(magic, "IMGS", 4) != 0) {
        std::cout << "Invalid file header\n";
        return false;
    }

    uint32_t version = 0;
    ifs.read(reinterpret_cast<char*>(&version), sizeof(version));
    if (version != 1) {
        std::cout << "Unsupported version\n";
        return false;
    }

    uint32_t count = 0;
    ifs.read(reinterpret_cast<char*>(&count), sizeof(count));
    if (count != ImGuiCol_COUNT) {
        std::cout << "Color count mismatch\n";
        return false;
    }

    colors.resize(count);
    for (uint32_t i = 0; i < count; ++i) {
        ifs.read(reinterpret_cast<char*>(&colors[i].x), sizeof(float));
        ifs.read(reinterpret_cast<char*>(&colors[i].y), sizeof(float));
        ifs.read(reinterpret_cast<char*>(&colors[i].z), sizeof(float));
        ifs.read(reinterpret_cast<char*>(&colors[i].w), sizeof(float));
        if (ifs.fail()) {
            std::cout << "Failed to read color " << i << "\n";
            return false;
        }
    }

    ifs.close();
    return true;
}

bool SaveStyleToScale(const char* filename, const std::vector<ImVec4>& colors) {
    std::ofstream ofs(filename, std::ios::binary);
    if (!ofs.is_open()) {
        std::cout << "Failed to open file " << filename << " for writing\n";
        return false;
    }

    ofs.write("IMGS", 4);

    uint32_t version = 1;
    ofs.write(reinterpret_cast<const char*>(&version), sizeof(version));

    uint32_t count = (uint32_t)colors.size();
    ofs.write(reinterpret_cast<const char*>(&count), sizeof(count));

    for (const auto& c : colors) {
        ofs.write(reinterpret_cast<const char*>(&c.x), sizeof(float));
        ofs.write(reinterpret_cast<const char*>(&c.y), sizeof(float));
        ofs.write(reinterpret_cast<const char*>(&c.z), sizeof(float));
        ofs.write(reinterpret_cast<const char*>(&c.w), sizeof(float));
    }

    ofs.close();
    return true;
}

void PrintColors(const std::vector<ImVec4>& colors) {
    for (size_t i = 0; i < colors.size(); ++i) {
        const ImVec4& c = colors[i];
        std::cout << i << ": " << ImGuiColNames[i] << " = ("
                  << c.x << ", " << c.y << ", " << c.z << ", " << c.w << ")\n";
    }
}

int main(int argc, char** argv) {
    const char* filename = "";
    if (argc > 1) {
        filename = argv[1];
    }
    
    std::vector<ImVec4> colors;

    if (!LoadStyleFromScale(filename, colors)) {
        std::cout << "Loading failed.\n";
        return 1;
    }

    std::cout << "Loaded colors:\n";
    PrintColors(colors);

    while (true) {
        std::cout << "\nEnter color index to edit (-1 to quit): ";
        int idx;
        std::cin >> idx;
        if (idx < 0 || idx >= (int)colors.size())
            break;

        ImVec4& c = colors[idx];
        std::cout << "Current value of " << ImGuiColNames[idx] << ": ("
                  << c.x << ", " << c.y << ", " << c.z << ", " << c.w << ")\n";

        std::cout << "Enter new RGBA values (0.0 to 1.0) separated by spaces: ";
        float r, g, b, a;
        std::cin >> r >> g >> b >> a;

        // Clamp values to 0..1
        r = (r < 0.0f) ? 0.0f : ((r > 1.0f) ? 1.0f : r);
        g = (g < 0.0f) ? 0.0f : ((g > 1.0f) ? 1.0f : g);
        b = (b < 0.0f) ? 0.0f : ((b > 1.0f) ? 1.0f : b);
        a = (a < 0.0f) ? 0.0f : ((a > 1.0f) ? 1.0f : a);

        c = {r, g, b, a};

        std::cout << "Color updated.\n";
    }

    if (!SaveStyleToScale(filename, colors)) {
        std::cout << "Failed to save file.\n";
        return 1;
    }

    std::cout << "Saved colors to " << filename << "\n";

    return 0;
}
