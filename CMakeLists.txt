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
# Zstd library (compile from sources)
# -----------------------------------------------------------------------------
file(GLOB_RECURSE ZSTD_C_SOURCES
    src/lib/zstd/*.c
)

set(ZSTD_ASM_SOURCES
    src/lib/zstd/decompress/huf_decompress_amd64.S
)

add_library(zstd STATIC ${ZSTD_C_SOURCES} ${ZSTD_ASM_SOURCES})

target_include_directories(zstd PUBLIC src/lib/zstd/include)

# Optional: specify CPU-enabled flags
#target_compile_options(zstd PUBLIC -msse2 -mavx2)


# -----------------------------------------------------------------------------
# Find system libraries
# -----------------------------------------------------------------------------
find_package(OpenGL REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_search_module(GLFW REQUIRED glfw3)

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
    ${GLFW_LIBRARIES}
    OpenGL::GL
    zstd
)

set_target_properties(angler PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${BIN_DIR}
)

# -----------------------------------------------------------------------------
# Post-build DLL copy (for Windows)
# -----------------------------------------------------------------------------
function(copy_dll target dll_path)
    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${dll_path}"
        "$<TARGET_FILE_DIR:${target}>"
    )
endfunction()

if (GLFW_FOUND)
    set(GLFW_DLL "C:/path/to/glfw3.dll") # Adjust this path to your system if needed
    if (EXISTS ${GLFW_DLL})
        copy_dll(angler ${GLFW_DLL})
    else()
        message(WARNING "GLFW DLL not found: ${GLFW_DLL}, skipping copy.")
    endif()
endif()

# You likely won't need these unless you build zstd as shared
if (FALSE)
    set(ZSTD_DLL "C:/path/to/zstd.dll") # Adjust path if needed
    if (EXISTS ${ZSTD_DLL})
        copy_dll(angler ${ZSTD_DLL})
    else()
        message(WARNING "ZSTD DLL not found: ${ZSTD_DLL}, skipping copy.")
    endif()
endif()


add_custom_command(TARGET angler POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/src/core/assets
        ${BIN_DIR}/assets
    COMMENT "Copying assets to build directory"
)
