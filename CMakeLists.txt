cmake_minimum_required(VERSION 3.10)

project(pillow)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")

set(pillow_static ON) 
if(pillow_static) 
    add_compile_definitions(pillow_static_export)
    set(BUILD_SHARED_LIBS OFF)
else()
    set(BUILD_SHARED_LIBS ON)
endif()

# Third-Party
set(SDL_SHARED FALSE)
set(SDL_STATIC TRUE)
include(FetchContent)
FetchContent_Declare(
    SDL
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG release-3.2.24
    GIT_PROGRESS TRUE
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(SDL)


# Utility 
add_library(utility INTERFACE)
target_include_directories(utility INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/utility")
target_sources(utility PRIVATE "utility/pillow_array.h")
target_sources(utility PRIVATE "utility/pillow_allocator.h")

# Executable 
add_library(executable INTERFACE)
target_include_directories(executable INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/executable")
target_sources(executable PRIVATE "executable/pillow_executable.h")

# Nuklear 
add_library(nuklear)
target_include_directories(nuklear PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/nuklear/include")
target_sources(nuklear PRIVATE "nuklear/include/nuklear.h")
target_sources(nuklear PRIVATE "nuklear/include/nuklear_demo.h")
target_sources(nuklear PRIVATE "nuklear/include/nuklear_default.h")

target_sources(nuklear PRIVATE "nuklear/nuklear_style.c")
target_sources(nuklear PRIVATE "nuklear/nuklear.c")

target_sources(nuklear PRIVATE "nuklear/nuklear_demo_calculator.c")
target_sources(nuklear PRIVATE "nuklear/nuklear_demo_canvas.c")
target_sources(nuklear PRIVATE "nuklear/nuklear_demo_configurator.c")
target_sources(nuklear PRIVATE "nuklear/nuklear_demo_node_editor.c")
target_sources(nuklear PRIVATE "nuklear/nuklear_demo_overview.c")
target_sources(nuklear PRIVATE "nuklear/nuklear_demo_style.c")

# Registry
add_library(registry)
target_include_directories(registry PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/registry/include")
target_sources(registry PRIVATE "registry/include/pillow_registry.h")
target_sources(registry PRIVATE "registry/pillow_registry.c")
target_link_libraries(registry utility)

# Case
add_library(case)
target_include_directories(case PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/case/include")
target_sources(case PRIVATE "case/include/pillow_case.h")
target_sources(case PRIVATE "case/pillow_case.c")
target_sources(case PRIVATE "case/pillow_case_nuklear.c")
target_sources(case PRIVATE "case/pillow_case_nuklear.h")
target_link_libraries(case nuklear registry executable utility SDL3::SDL3)

# Main
add_executable(main)
target_sources(main PRIVATE "main/main.c")
target_link_libraries(main case registry executable utility)

# -- CMake - Folders -- 
set_target_properties(utility main nuklear PROPERTIES FOLDER "core")
set_target_properties(executable PROPERTIES FOLDER "interfaces")
set_target_properties(registry case PROPERTIES FOLDER "apis")