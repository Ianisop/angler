cmake_minimum_required(VERSION 3.14)
project(Angler)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
enable_language(ASM)

set(BIN_DIR ${CMAKE_BINARY_DIR}/bin)

# -----------------------------------------------------------------------------
# GLAD library
# -----------------------------------------------------------------------------
add_library(glad STATIC
    src/lib/glad/glad.c
)
target_include_directories(glad PUBLIC src/lib/glad)

# -----------------------------------------------------------------------------
# ImGui library
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
target_include_directories(imgui PUBLIC src/lib/imgui)

# -----------------------------------------------------------------------------
# Zstd library
# -----------------------------------------------------------------------------
file(GLOB_RECURSE ZSTD_C_SOURCES
    src/lib/zstd/*.c
)

set(ZSTD_ASM_SOURCES "")
if (NOT MSVC) # MSVC can't handle .S assembly files
    set(ZSTD_ASM_SOURCES
        src/lib/zstd/decompress/huf_decompress_amd64.S
    )
else()
    message(WARNING "Skipping Zstd ASM optimizations on MSVC")
endif()

add_library(zstd STATIC ${ZSTD_C_SOURCES} ${ZSTD_ASM_SOURCES})
target_include_directories(zstd PUBLIC src/lib/zstd/include)

# -----------------------------------------------------------------------------
# GLFW 
# -----------------------------------------------------------------------------
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)

add_subdirectory(src/lib/glfw)

# -----------------------------------------------------------------------------
# Find OpenGL
# -----------------------------------------------------------------------------
find_package(OpenGL REQUIRED)

# -----------------------------------------------------------------------------
# Main executable
# -----------------------------------------------------------------------------
add_executable(angler
    src/core/angler.cpp
    src/core/file_indexer.cpp
)

target_include_directories(angler PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    src/lib/zstd/
    external/
)

target_link_libraries(angler
    glad
    imgui
    glfw           
    OpenGL::GL
    zstd
)

if (WIN32 AND MSVC)
    target_link_libraries(angler opengl32)
endif()

set_target_properties(angler PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${BIN_DIR}
)

# -----------------------------------------------------------------------------
# Post-build: Copy assets
# -----------------------------------------------------------------------------
add_custom_command(TARGET angler POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/src/core/assets
        ${BIN_DIR}/assets
    COMMENT "Copying assets to build directory"
)
