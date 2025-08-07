cmake_minimum_required(VERSION 3.14)
project(Angler)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# -----------------------------------------------------------------------------
# Include directories
# -----------------------------------------------------------------------------

include_directories(
    src/lib/glad
    src/lib/imgui
)

# -----------------------------------------------------------------------------
# GLAD
# -----------------------------------------------------------------------------

add_library(glad
    src/lib/glad/glad.c
)

# -----------------------------------------------------------------------------
# ImGui
# -----------------------------------------------------------------------------

set(IMGUI_SRC
    src/lib/imgui/imgui.cpp
    src/lib/imgui/imgui_draw.cpp
    src/lib/imgui/imgui_widgets.cpp
    src/lib/imgui/imgui_tables.cpp
    src/lib/imgui/imgui_impl_glfw.cpp
    src/lib/imgui/imgui_impl_opengl3.cpp
)

add_library(imgui ${IMGUI_SRC})

# -----------------------------------------------------------------------------
# Find system libraries
# -----------------------------------------------------------------------------

find_package(OpenGL REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_search_module(GLFW REQUIRED glfw3)
include_directories(${GLFW_INCLUDE_DIRS})
link_libraries(${GLFW_LIBRARIES})


# -----------------------------------------------------------------------------
# Executable
# -----------------------------------------------------------------------------

add_executable(angler
    src/core/angler.cpp
)

target_link_libraries(angler
    glad
    imgui
    glfw
    OpenGL::GL
)
