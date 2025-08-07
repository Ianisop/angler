#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include "angler_widgets.h"
#include "texture_loader.cpp"
#include "style_manager.cpp"

#include <vector>
#include <iostream>

// -- SIDEBAR --
std::vector<Tab> tabs;
int currentTabIndex = -1;

void glfw_error_callback(int error, const char* description) {
    std::cerr << "GLFW Error " << error << ": " << description << std::endl;
}

int main() {
    // Setup GLFW
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW\n";
        return -1;
    }

    // OpenGL version: 3.3 Core
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE); // Required on Mac
#endif

    // Create window
    GLFWwindow* window = glfwCreateWindow(1280, 720, "Angler ImGui Demo", nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create GLFW window\n";
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // Enable vsync

    // Initialize GLAD
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Failed to initialize GLAD\n";
        return -1;
    }

    // Setup Dear ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;

    ImGui::StyleColorsDark();

    // Initialize ImGui platform/renderer backends
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    // Main loop
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        SaveStyleToScale("my_skin.scale");
        // Load style
        if (!LoadStyleFromScale("my_skin.scale")) {
            printf("Failed to load style\n");
        }
        // Start new frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        
        // --- IMGUI WIDGETS ---
        
        // Debug window
        ImGui::Begin("Hello, Angler!");
        ImGui::Text("DEBUG:");
        ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
        ImGui::End();

        ImVec2 windowSize = ImVec2(200, ImGui::GetIO().DisplaySize.y); // Sidebar width, full screen height
        ImVec2 windowPos = ImVec2(0, 0); // Right edge

        int my_tex_width = 16;
        int my_tex_height = 16;
        GLuint my_texture = LoadTextureFromFile("src/core/icons/file.png", &my_tex_width, &my_tex_height);

        ImGui::SetNextWindowPos(windowPos);
        ImGui::SetNextWindowSize(windowSize);
        ImGui::Begin("Sidebar", nullptr,
                    ImGuiWindowFlags_NoResize |
                    ImGuiWindowFlags_NoMove |
                    ImGuiWindowFlags_NoCollapse); // prevents folding

        ImGui::Image((void*)(intptr_t)my_texture, ImVec2(my_tex_width, my_tex_height));

        // Render existing tabs as buttons
        for (int i = 0; i < tabs.size(); ++i) {
            if (ImGui::Selectable(tabs[i].name.c_str(), currentTabIndex == i)) {
                currentTabIndex = i;
            }
        }

        // Add button to create new tabs TODO: use a file chooser for this
        if (ImGui::Button("+ Add Tab")) {
            static int tabCount = 1;
            tabs.push_back({ "Tab " + std::to_string(tabCount++) });
            currentTabIndex = tabs.size() - 1; // switch to new tab
        }

        ImGui::End();


        // Render
        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
