cmake_minimum_required(VERSION 3.12)

set(PROJECT_NAME ClayEngineVK)
project(${PROJECT_NAME})

# Specify C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Option to build tests (default OFF so users don't build them)
option(CLAYENGINE_BUILD_TESTS "Build ClayEngineVK tests" OFF)

# Option to build a game's permanent UI (menus, HUD) on RmlUi (thirdparty/RmlUi submodule,
# MIT): UiComponentDesktop / clay::uibridge, implemented by
# src/clay/gui/desktop/RmlUiRenderBridge.cpp. OFF by default: the UI functions are then no-ops.
option(CLAY_ENABLE_RMLUI "Build the game UI on RmlUi (thirdparty/RmlUi)" OFF)

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
# vulkan
find_package(Vulkan REQUIRED) 
# PhysX
set(CMAKE_EXE_LINKER_FLAGS_CHECKED "" CACHE STRING "" FORCE)
set(CMAKE_EXE_LINKER_FLAGS_PROFILE "" CACHE STRING "" FORCE)
set(PHYSX_BUILD_DEBUG TRUE CACHE BOOL "Force PhysX debug" FORCE)
set(CMAKEMODULES_VERSION "1.27")
set(CMAKE_INSTALL_PREFIX ${CMAKE_BINARY_DIR}/physx_bin CACHE PATH "" FORCE)
set(PXSHARED_INSTALL_PREFIX ${CMAKE_INSTALL_PREFIX})
set(PHYSX_ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/PhysX/physx CACHE PATH "" FORCE)
set(PXSHARED_PATH ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/PhysX/pxshared CACHE PATH "" FORCE)
set(CMAKEMODULES_PATH ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/PhysX/externals/cmakemodules CACHE PATH "" FORCE)
set(PX_DISABLE_WARNINGS_AS_ERRORS TRUE CACHE BOOL "" FORCE)
set(PX_GENERATE_STATIC_LIBRARIES TRUE CACHE BOOL "" FORCE)
set(PHYSX_LIBTYPE STATIC CACHE STRING "Build PhysX as static library")
set(PX_BUILD_TESTS FALSE CACHE BOOL "" FORCE)
set(PX_BUILDPUBLICSAMPLES FALSE CACHE BOOL "" FORCE)
set(PX_BUILDSNIPPETS FALSE CACHE BOOL "" FORCE)
# Output directories for PhysX build artifacts
set(PX_OUTPUT_BIN_DIR ${CMAKE_BINARY_DIR}/physx/bin CACHE PATH "" FORCE)
set(PX_OUTPUT_LIB_DIR ${CMAKE_BINARY_DIR}/physx/lib CACHE PATH "" FORCE)
set(PX_OUTPUT_DLL_DIR ${CMAKE_BINARY_DIR}/physx/bin CACHE PATH "" FORCE)

if(CLAY_PLATFORM_DESKTOP)
    if (WIN32)
        # Windows
        add_compile_options("/Wv:18")
        set(NV_USE_DEBUG_WINCRT ON CACHE BOOL "Use the debug version of the CRT")
        set(TARGET_BUILD_PLATFORM "windows" CACHE STRING "Target platform for PhysX" FORCE)
    elseif (UNIX)
        # Unix
        set(TARGET_BUILD_PLATFORM "linux" CACHE STRING "Target platform for PhysX" FORCE)
        add_compile_options(
            -Wno-array-bounds
            -Wno-error=cpp
            -Wno-error=volatile
            -Wno-error=class-memaccess
            -Wno-error=deprecated-enum-enum-conversion
            -Wno-error=nonnull
            -Wno-error=restrict
            -Wno-error=template-id-cdtor
            -Wno-error=deprecated-enum-float-conversion
        )
    endif()
elseif (CLAY_PLATFORM_ANDROID)
    set(TARGET_BUILD_PLATFORM "android" CACHE STRING "Target platform for PhysX" FORCE)
    set(ANDROID_ABI "armeabi-v7a with NEON" CACHE STRING "Android ABI" FORCE)
    set(ANDROID_PLATFORM android-21 CACHE STRING "Android API level" FORCE)
    set(ANDROID_STL c++_static CACHE STRING "Android STL" FORCE)
elseif (CLAY_PLATFORM_XR)
    set(TARGET_BUILD_PLATFORM "android" CACHE STRING "Target platform for PhysX" FORCE)
    set(ANDROID_ABI "armeabi-v7a with NEON" CACHE STRING "Android ABI" FORCE)
    set(ANDROID_PLATFORM android-21 CACHE STRING "Android API level" FORCE)
    set(ANDROID_STL c++_static CACHE STRING "Android STL" FORCE)
endif()

# Add third party directories
# Assimp
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/assimp)
# FreeType
add_subdirectory(
    ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/freetype
    ${CMAKE_BINARY_DIR}/thirdparty/freetype
)
# libsndfile
add_subdirectory(
    ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/libsndfile
    ${CMAKE_BINARY_DIR}/thirdparty/libsndfile
)
# OpenAL
add_subdirectory(
    ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/openal-soft
    ${CMAKE_BINARY_DIR}/thirdparty/openal-soft
)
# Physx
add_subdirectory(
    ${PHYSX_ROOT_DIR}/compiler/public
    ${CMAKE_BINARY_DIR}/thirdparty/PhysX
)
# OpenXR
if(CLAY_PLATFORM_XR)
    add_subdirectory(
        ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/OpenXR-SDK-Source
        ${CMAKE_BINARY_DIR}/thirdparty/OpenXR-SDK-Source
    )
endif()
# GLFW
if(CLAY_PLATFORM_DESKTOP)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/glfw)
endif()

file(GLOB_RECURSE SRC_FILES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)

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
    ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/json/include
    ${PHYSX_ROOT_DIR}/include
    ${PXSHARED_PATH}/include
)

if(CLAY_PLATFORM_XR OR CLAY_PLATFORM_ANDROID)
    target_include_directories(${PROJECT_NAME} PUBLIC
        ${ANDROID_NDK}/sources/android/native_app_glue
        ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/Vulkan-Hpp
    )
endif()
if(CLAY_PLATFORM_DESKTOP)
    target_include_directories(${PROJECT_NAME} PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/stb
    )
endif()
# stb_vorbis (Ogg Vorbis decoding, clay/audio/Audio.cpp) is needed on every platform.
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/stb
)

# Define common libraries
set(CLAY_COMMON_LIBS
    assimp
    freetype
    OpenAL
    PhysXExtensions
    PhysX
    PhysXPvdSDK
    PhysXCommon
    PhysXFoundation
    sndfile
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
elseif(CLAY_PLATFORM_ANDROID)
    set(CLAY_PLATFORM_LIBS
        android
        vulkan
        log
        native_app_glue
        jnigraphics
    )
elseif (CLAY_PLATFORM_DESKTOP)
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

# Link platform-specific and common libraries
target_link_libraries(${PROJECT_NAME} PUBLIC ${CLAY_COMMON_LIBS} ${CLAY_PLATFORM_LIBS})
target_compile_definitions(${PROJECT_NAME} PUBLIC VULKAN_HPP_NO_STRUCT_CONSTRUCTORS)

if(CLAY_PLATFORM_XR)
    target_compile_definitions(${PROJECT_NAME} PUBLIC CLAY_PLATFORM_XR)
    # PhysX needs exactly one of _DEBUG / NDEBUG: optimised configs (the APK's release
    # variant = RelWithDebInfo) already define NDEBUG.
    target_compile_definitions(${PROJECT_NAME} PUBLIC $<$<CONFIG:Debug>:_DEBUG>)
endif()
if(CLAY_PLATFORM_ANDROID)
    target_compile_definitions(${PROJECT_NAME} PUBLIC CLAY_PLATFORM_ANDROID)
    # PhysX needs exactly one of _DEBUG / NDEBUG: optimised configs already define NDEBUG.
    target_compile_definitions(${PROJECT_NAME} PUBLIC $<$<CONFIG:Debug>:_DEBUG>)
endif()
if(CLAY_PLATFORM_DESKTOP)
    target_compile_definitions(${PROJECT_NAME} PUBLIC CLAY_PLATFORM_DESKTOP)
    target_compile_definitions(${PROJECT_NAME} PUBLIC PX_PHYSX_STATIC_LIB)
endif()

# ---------------------------------------------------------------------------
# RmlUi (optional, desktop + Android + XR / Vulkan): HTML/CSS-like UI, statically linked, using the engine's
# FreeType. The Vulkan renderer for it is the engine's own (RmlUiRenderBridge.cpp).
# ---------------------------------------------------------------------------
if(CLAY_ENABLE_RMLUI AND (CLAY_PLATFORM_DESKTOP OR CLAY_PLATFORM_ANDROID OR CLAY_PLATFORM_XR))
    if(NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/RmlUi/CMakeLists.txt)
        message(FATAL_ERROR "CLAY_ENABLE_RMLUI=ON but thirdparty/RmlUi is missing (git submodule update --init thirdparty/RmlUi).")
    endif()
    # RmlUi looks for Freetype::Freetype; give it the in-tree build (and skip the system search).
    if(NOT TARGET Freetype::Freetype)
        add_library(Freetype::Freetype ALIAS freetype)
    endif()
    set(CMAKE_DISABLE_FIND_PACKAGE_Freetype TRUE)
    set(RMLUI_FONT_ENGINE "freetype" CACHE STRING "" FORCE)
    set(RMLUI_SAMPLES OFF CACHE BOOL "" FORCE)
    set(RMLUI_PRECOMPILED_HEADERS OFF CACHE BOOL "" FORCE)
    set(_clay_shared ${BUILD_SHARED_LIBS})
    set(BUILD_SHARED_LIBS OFF)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/RmlUi ${CMAKE_BINARY_DIR}/thirdparty/RmlUi EXCLUDE_FROM_ALL)
    set(BUILD_SHARED_LIBS ${_clay_shared})
    set(CMAKE_DISABLE_FIND_PACKAGE_Freetype FALSE)
    target_link_libraries(${PROJECT_NAME} PUBLIC RmlUi::Core)
    target_compile_definitions(${PROJECT_NAME} PUBLIC CLAY_ENABLE_RMLUI)
    message(STATUS "RmlUi: enabled (thirdparty/RmlUi)")
endif()


# Build tests if requested
if(CLAYENGINE_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()