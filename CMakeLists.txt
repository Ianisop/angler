cmake_minimum_required(VERSION 3.14)
project(Angler)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Output directory for binaries
set(BIN_DIR ${CMAKE_BINARY_DIR}/bin)

# Include directories
include_directories(
    src/lib/glad
    src/lib/imgui
    external/
)

# -----------------------------------------------------------------------------
# GLAD
# -----------------------------------------------------------------------------
add_library(glad STATIC
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
add_library(imgui STATIC ${IMGUI_SRC})

# -----------------------------------------------------------------------------
# Find system packages
# -----------------------------------------------------------------------------

find_package(OpenGL REQUIRED)

# Find GLFW shared (DLL)
find_package(PkgConfig REQUIRED)
pkg_search_module(GLFW REQUIRED glfw3)

# Assume GLFW libraries from pkg_search_module includes the DLL
include_directories(${GLFW_INCLUDE_DIRS})

# Find zstd shared library
find_library(ZSTD_LIBRARY zstd)
find_path(ZSTD_INCLUDE_DIR zstd.h)
include_directories(${ZSTD_INCLUDE_DIR})

# -----------------------------------------------------------------------------
# Executable
# -----------------------------------------------------------------------------

add_executable(angler
    src/core/angler.cpp
    src/core/file_indexer.cpp
)

target_link_libraries(angler
    glad
    imgui
    ${GLFW_LIBRARIES}
    OpenGL::GL
    ${ZSTD_LIBRARY}
)

# -----------------------------------------------------------------------------
# Post-build: Copy DLLs to output dir
# -----------------------------------------------------------------------------

# Set output directory for executable and DLLs
set_target_properties(angler PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${BIN_DIR}
)

# Helper function to copy DLLs after build
function(copy_dll target dll_path)
    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${dll_path}"
        "$<TARGET_FILE_DIR:${target}>"
    )
endfunction()

# Example: You need to know exact paths of DLLs to copy
# You can adjust these to your actual DLL locations

# Copy GLFW DLL
if (GLFW_FOUND)
    # pkg_search_module GLFW doesn't give DLL path by default, set manually if needed
    set(GLFW_DLL "C:/path/to/glfw3.dll") # Change to your actual DLL location
    if (EXISTS ${GLFW_DLL})
        copy_dll(angler ${GLFW_DLL})
    else()
        message(WARNING "GLFW DLL not found: ${GLFW_DLL}, skipping copy.")
    endif()
endif()

# Copy zstd DLL
if (ZSTD_LIBRARY)
    # Guess DLL path by replacing .lib/.a with .dll or specify manually
    # On Windows, if you have zstd.dll somewhere:
    set(ZSTD_DLL "C:/path/to/zstd.dll") # Change this to your actual DLL path
    if (EXISTS ${ZSTD_DLL})
        copy_dll(angler ${ZSTD_DLL})
    else()
        message(WARNING "ZSTD DLL not found: ${ZSTD_DLL}, skipping copy.")
    endif()
endif()
