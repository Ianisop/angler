#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include "angler_widgets.h"
#include "style_manager.cpp"
#include "icons.h"


#include <vector>
#include <iostream>



// -- GLOBALS --
#define FONT_SIZE 20
GLFWwindow* window;
int display_w, display_h;
static bool dragging = false;
static ImVec2 drag_offset;


// -- SIDEBAR --
std::vector<Tab> tabs;
int current_tab_index = -1;
bool scale_loaded = false;



void glfw_error_callback(int error, const char* description) {
    std::cerr << "GLFW Error " << error << ": " << description << std::endl;
}




void RunAnglerWidgets() {
    // Start new frame
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    // Handle font early on
    ImFont* font = ImGui::GetIO().Fonts->AddFontFromFileTTF("src/core/assets/karla.ttf",FONT_SIZE);
    ImGui::PushFont(font);

    // Load style
    if (!scale_loaded && !LoadStyleFromScale("my_skin.scale")) {
        printf("Failed to load style\n");
    }
    scale_loaded = true;

    // Layout constants
    float toolbar_height = 30.0f;
    float sidebar_width = ImGui::GetIO().DisplaySize.x * 0.18f; // 18% of screen width
    float screen_width = ImGui::GetIO().DisplaySize.x;
    float screen_height = ImGui::GetIO().DisplaySize.y;

    // TOP TOOLBAR
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(screen_width, toolbar_height));

    ImGuiWindowFlags toolbar_flags = ImGuiWindowFlags_NoDecoration |
                                     ImGuiWindowFlags_NoMove |
                                     ImGuiWindowFlags_NoSavedSettings |
                                     ImGuiWindowFlags_NoBringToFrontOnFocus;

    ImGui::Begin("Angler", nullptr, toolbar_flags);

    // Drag-to-move
    ImGuiIO& io = ImGui::GetIO();
    //std::cout << "dragging:" << dragging << "\n";

    // Get toolbar rect
    ImVec2 toolbar_pos = ImGui::GetWindowPos();
    ImVec2 toolbar_size = ImGui::GetWindowSize();
    ImVec2 mouse_pos = io.MousePos;
    
    // Check if mouse is inside the toolbar
    bool mouse_in_toolbar = mouse_pos.x >= toolbar_pos.x && mouse_pos.x <= (toolbar_pos.x + toolbar_size.x) &&
                          mouse_pos.y >= toolbar_pos.y && mouse_pos.y <= (toolbar_pos.y + toolbar_size.y);
    
    // Start dragging if left mouse pressed in toolbar
    if (!dragging && mouse_in_toolbar && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        dragging = true;
    }
    
    // Stop dragging when mouse is released
    if (dragging && !ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        dragging = false;
    }
    
    // Perform dragging
    if (dragging) {
        ImVec2 delta = io.MouseDelta;
        int wx, wy;
        glfwGetWindowPos(window, &wx, &wy);
        glfwSetWindowPos(window, wx + (int)delta.x, wy + (int)delta.y);
    }
    
    ImGui::SameLine(ImGui::GetWindowWidth() / 2);

    ImGui::Text("Angler");

    ImGui::SameLine(ImGui::GetWindowWidth() - 90);
    if (ImGui::Button("_")) {
        // Minimize
    }
    ImGui::SameLine();
    if (ImGui::Button("□")) {
        // Maximize toggle
    }
    ImGui::SameLine();
    if (ImGui::Button("X")) {
        glfwSetWindowShouldClose(window, true);
    }

    ImGui::End();

    // SIDEBAR
    ImVec2 sidebar_size = ImVec2(sidebar_width, screen_height - toolbar_height);
    ImVec2 sidebar_pos = ImVec2(0, toolbar_height);
    ImGui::SetNextWindowPos(sidebar_pos);
    ImGui::SetNextWindowSize(sidebar_size);

    ImGui::Begin(" ", nullptr,
                 ImGuiWindowFlags_NoTitleBar |
                 ImGuiWindowFlags_NoResize |
                 ImGuiWindowFlags_NoMove |
                 ImGuiWindowFlags_NoCollapse);
    
    auto my_texture = Icons::FetchIconTextureByType(Icons::VIDEO,64, &Icons::ICON_SIZE_SMALL, &Icons::ICON_SIZE_SMALL);
    ImGui::Image((void*)(intptr_t)my_texture, ImVec2(Icons::ICON_SIZE_SMALL, Icons::ICON_SIZE_SMALL));

    for (int i = 0; i < tabs.size(); ++i) {
        if (ImGui::Selectable(tabs[i].name.c_str(), current_tab_index == i)) {
            current_tab_index = i;
        }
    }

    if (ImGui::Button("+ Add Tab")) {
        static int tab_count = 1;
        tabs.push_back({ "Tab " + std::to_string(tab_count++) });
        current_tab_index = tabs.size() - 1;
    }

    ImGui::End();

    // RIGHT PANE
    ImVec4 original_color = ImGui::GetStyleColorVec4(ImGuiCol_WindowBg);
    ImVec4 darker_color = ImVec4(
        original_color.x * 0.9f,
        original_color.y * 0.9f,
        original_color.z * 0.9f,
        original_color.w
    );
    ImGui::PushStyleColor(ImGuiCol_WindowBg, darker_color);

    ImVec2 right_pane_pos = ImVec2(sidebar_width, toolbar_height);
    ImVec2 right_pane_size = ImVec2(screen_width - sidebar_width, screen_height - toolbar_height);

    ImGui::SetNextWindowPos(right_pane_pos);
    ImGui::SetNextWindowSize(right_pane_size);

    ImGui::Begin("Main", nullptr,
                 ImGuiWindowFlags_NoResize |
                 ImGuiWindowFlags_NoMove |
                 ImGuiWindowFlags_NoCollapse |
                 ImGuiWindowFlags_NoTitleBar);



    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopFont();
    // Render
    ImGui::Render();
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
    glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE); // Required on Mac
#endif

    // Create window
    window = glfwCreateWindow(1280, 720, "Angler ImGui Demo", nullptr, nullptr);
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



    // Initialize ImGui platform/renderer backends
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    // Main loop
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();



        RunAnglerWidgets();

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
