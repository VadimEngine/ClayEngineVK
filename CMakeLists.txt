cmake_minimum_required(VERSION 3.12)

set(PROJECT_NAME ClayEngineVK)
project(${PROJECT_NAME})

# Specify C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Assimp, sndfile, glfw
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build shared libraries" FORCE)
# assimp
set(ASSIMP_BUILD_TESTS OFF CACHE BOOL "If the test suite for Assimp is built in addition to the library." FORCE)
set(ASSIMP_INSTALL OFF CACHE BOOL "Disable this if you want to use assimp as a submodule." FORCE)
# GLEW
set(ONLY_LIBS ON CACHE BOOL "Do not build executables")
# GLFW
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "Build the GLFW example programs" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "Build the GLFW test programs" FORCE)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "Build the GLFW documentation" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "Generate installation target" FORCE)
# sndfile
set(BUILD_PROGRAMS OFF CACHE BOOL "Build program" FORCE)
set(BUILD_EXAMPLES OFF CACHE BOOL "Build examples" FORCE)
set(BUILD_TESTING OFF CACHE BOOL "build tests" FORCE)
# Open AL
set(LIBTYPE STATIC CACHE STRING "Build OpenAL as a static library" FORCE)
set(ALSOFT_TESTS OFF CACHE BOOL "Build test programs")
set(ALSOFT_EXAMPLES OFF CACHE BOOL "Build example program")
# OpenXR
set(BUILD_TESTS OFF CACHE INTERNAL "Disable OpenXR tests" FORCE)
set(BUILD_API_LAYERS ON CACHE INTERNAL "Enable OpenXR API layers" FORCE)

# Add third party directories
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/assimp)
add_subdirectory(
    ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/freetype
    ${CMAKE_BINARY_DIR}/thirdparty/freetype
)
add_subdirectory(
    ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/libsndfile
    ${CMAKE_BINARY_DIR}/thirdparty/libsndfile
)
add_subdirectory(
    ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/openal-soft
    ${CMAKE_BINARY_DIR}/thirdparty/openal-soft
)
if(CLAY_PLATFORM_XR)
    add_subdirectory(
        ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/OpenXR-SDK-Source
        ${CMAKE_BINARY_DIR}/thirdparty/OpenXR-SDK-Source
    )
endif()
if(CLAY_PLATFORM_DESKTOP)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/glfw)
endif()

file(GLOB_RECURSE SRC_FILES ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)

# Add imgui source files
list(APPEND SRC_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/imgui/imgui.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/imgui/imgui_demo.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/imgui/imgui_draw.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/imgui/backends/imgui_impl_vulkan.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/imgui/imgui_tables.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/imgui/imgui_widgets.cpp
)
if(CLAY_PLATFORM_XR OR CLAY_PLATFORM_ANDROID)
    list(APPEND SRC_FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/imgui/backends/imgui_impl_android.cpp
    )
endif()
if(CLAY_PLATFORM_DESKTOP)
    list(APPEND SRC_FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/imgui/backends/imgui_impl_glfw.cpp
    )
endif()

# Define the library target
add_library(${PROJECT_NAME} STATIC ${SRC_FILES})
if(CLAY_PLATFORM_XR OR CLAY_PLATFORM_ANDROID)
    # Android-specific imports
    include(AndroidNdkModules)
    android_ndk_import_module_native_app_glue()
endif()

# Include directories for public headers
target_include_directories(${PROJECT_NAME} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/glm
    ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/imgui
    ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/imgui/backends
)

if(CLAY_PLATFORM_XR OR CLAY_PLATFORM_ANDROID)
    target_include_directories(${PROJECT_NAME} PUBLIC
        ${ANDROID_NDK}/sources/android/native_app_glue
    )
endif()
if(CLAY_PLATFORM_DESKTOP)
    target_include_directories(${PROJECT_NAME} PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/stb
    )
endif()

# Define common libraries
set(CLAY_COMMON_LIBS
    freetype
    sndfile
    OpenAL
    assimp
)

if(CLAY_PLATFORM_XR)
    set(CLAY_PLATFORM_LIBS
        android
        vulkan
        log
        native_app_glue
        jnigraphics
        openxr_loader
    )
endif()
if(CLAY_PLATFORM_ANDROID)
    set(CLAY_PLATFORM_LIBS
        android
        vulkan
        log
        native_app_glue
        jnigraphics
    )
endif()
if (CLAY_PLATFORM_DESKTOP) # todo elseif?
    # Common desktop libraries
    list(APPEND CLAY_PLATFORM_LIBS
        glfw
        Vulkan::Vulkan
    )
    if (WIN32)
        # Windows
        list(APPEND CLAY_PLATFORM_LIBS
            winmm
            avrt
        )
    elseif (UNIX)
        # Unix
        list(APPEND CLAY_PLATFORM_LIBS
            pthread
            X11
        )
    endif()
endif()

# TODO confirm this for android
find_package(Vulkan REQUIRED) 

# Link platform-specific and common libraries
target_link_libraries(${PROJECT_NAME} PUBLIC ${CLAY_COMMON_LIBS} ${CLAY_PLATFORM_LIBS})

if(CLAY_PLATFORM_XR)
    target_compile_definitions(${PROJECT_NAME} PUBLIC CLAY_PLATFORM_XR)
endif()
if(CLAY_PLATFORM_ANDROID)
    target_compile_definitions(${PROJECT_NAME} PUBLIC CLAY_PLATFORM_ANDROID)
endif()
if(CLAY_PLATFORM_DESKTOP)
    target_compile_definitions(${PROJECT_NAME} PUBLIC CLAY_PLATFORM_DESKTOP)
endif()