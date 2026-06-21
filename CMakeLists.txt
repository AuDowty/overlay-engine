cmake_minimum_required(VERSION 3.20)
project(overlay-engine VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT WIN32)
    message(FATAL_ERROR "overlay-engine is Windows-only")
endif()

include(FetchContent)

FetchContent_Declare(
    microhook
    GIT_REPOSITORY https://github.com/AuDowty/microhook.git
    GIT_TAG main)
FetchContent_MakeAvailable(microhook)

FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.91.5)
FetchContent_MakeAvailable(imgui)

add_library(imgui_static STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_win32.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_dx11.cpp)
target_include_directories(imgui_static PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends)
target_compile_features(imgui_static PUBLIC cxx_std_20)

add_library(overlay-engine STATIC
    src/overlay.cpp
    src/backend_dx11.cpp
    src/window_hook.cpp)
target_include_directories(overlay-engine PUBLIC include)
target_link_libraries(overlay-engine PUBLIC
    microhook
    imgui_static
    d3d11
    dxgi
    dwmapi)
target_compile_features(overlay-engine PUBLIC cxx_std_20)
if(MSVC)
    target_compile_options(overlay-engine PRIVATE /W4 /permissive- /EHsc)
endif()

option(OVERLAY_BUILD_EXAMPLES "Build examples" ON)
if(OVERLAY_BUILD_EXAMPLES)
    add_executable(dx11_demo examples/dx11_demo/main.cpp)
    target_link_libraries(dx11_demo PRIVATE overlay-engine)
    if(MSVC)
        target_compile_options(dx11_demo PRIVATE /W4 /permissive- /EHsc)
    endif()
endif()
