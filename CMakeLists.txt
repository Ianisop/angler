cmake_minimum_required(VERSION 3.14)
project(Angler)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

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
# System packages
# -----------------------------------------------------------------------------
find_package(OpenGL REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_search_module(GLFW REQUIRED glfw3)
include_directories(${GLFW_INCLUDE_DIRS})

find_library(ZSTD_LIBRARY zstd)
find_path(ZSTD_INCLUDE_DIR zstd.h)
include_directories(${ZSTD_INCLUDE_DIR})



# -----------------------------------------------------------------------------
# angler main executable
# -----------------------------------------------------------------------------
add_executable(angler
    src/core/angler.cpp
    src/core/file_indexer.cpp
)

target_include_directories(angler PRIVATE ${CMAKE_SOURCE_DIR}/src)

set_target_properties(angler PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${BIN_DIR}
)

target_link_libraries(angler
    glad
    imgui
    ${GLFW_LIBRARIES}
    OpenGL::GL
    ${ZSTD_LIBRARY}
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
    set(GLFW_DLL "C:/path/to/glfw3.dll") # Adjust for your system
    if (EXISTS ${GLFW_DLL})
        copy_dll(angler ${GLFW_DLL})
    else()
        message(WARNING "GLFW DLL not found: ${GLFW_DLL}, skipping copy.")
    endif()
endif()

if (ZSTD_LIBRARY)
    set(ZSTD_DLL "C:/path/to/zstd.dll") # Adjust for your system
    if (EXISTS ${ZSTD_DLL})
        copy_dll(angler ${ZSTD_DLL})
    else()
        message(WARNING "ZSTD DLL not found: ${ZSTD_DLL}, skipping copy.")
    endif()
endif()
