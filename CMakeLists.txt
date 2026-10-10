cmake_minimum_required(VERSION 3.20)
project(CSOPESY_OS_Emulator LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

# GLFW
FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG        3.3.8
)
set(GLFW_BUILD_DOCS     OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(glfw)

# Dear ImGui (header-only fetch, compiled as static lib below)
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        v1.90.4
)
FetchContent_GetProperties(imgui)
if(NOT imgui_POPULATED)
    FetchContent_Populate(imgui)
endif()

# stb (header-only)
FetchContent_Declare(
    stb
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_TAG        master
)
FetchContent_GetProperties(stb)
if(NOT stb_POPULATED)
    FetchContent_Populate(stb)
endif()

# Build Dear ImGui as a static library
add_library(imgui_lib STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
    ${imgui_SOURCE_DIR}/misc/cpp/imgui_stdlib.cpp
)
target_include_directories(imgui_lib PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)
# Lets us write ImVec2 math (a + b, a * 0.5f) everywhere.
target_compile_definitions(imgui_lib PUBLIC IMGUI_DEFINE_MATH_OPERATORS)
target_link_libraries(imgui_lib PUBLIC glfw)

# Find OpenGL
find_package(OpenGL REQUIRED)

# Main executable
add_executable(${PROJECT_NAME}
    src/core/main.cpp
    src/core/application.cpp
    src/core/config.cpp
    src/core/texture_cache.cpp
    src/fs/virtual_fs.cpp
    src/os/process_manager.cpp
    src/os/shell.cpp
    src/apps/app.cpp
    src/apps/file_explorer.cpp
    src/apps/notepad.cpp
    src/apps/terminal.cpp
    src/apps/task_manager.cpp
    src/apps/settings_app.cpp
    src/apps/system_info.cpp
    src/screens/bios_screen.cpp
    src/screens/loading_screen.cpp
    src/screens/desktop.cpp
    src/screens/shutdown_screen.cpp
    src/ui/icons.cpp
    src/ui/quick_settings.cpp
    src/ui/taskbar.cpp
    src/ui/wallpaper.cpp
    src/lib/stb_image_impl.cpp
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${stb_SOURCE_DIR}
    src
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    imgui_lib
    OpenGL::GL
)

# Copy wallpapers and config.txt next to the executable
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/wallpapers"
        "$<TARGET_FILE_DIR:${PROJECT_NAME}>/wallpapers"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/config.txt"
        "$<TARGET_FILE_DIR:${PROJECT_NAME}>/config.txt"
)

# Visual Studio: Run/Debug starts the emulator (not ALL_BUILD) from the project
# folder, so edits to config.txt there apply without rebuilding.
set_property(DIRECTORY ${CMAKE_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT ${PROJECT_NAME})
set_target_properties(${PROJECT_NAME} PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
