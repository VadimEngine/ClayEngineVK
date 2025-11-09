# Clay Engine with Vulkan

ClayEngineVK is a static C++ game engine library built on Vulkan, designed for cross-platform development with support for Windows, Linux, Android, and Oculus Quest XR.

### Third Party Libraries
All the libraries submodules are included in `./thirdparty`:

- assimp
    - Loading 3d models and converting them to renderable components 
- freetype
    - Loading and rendering fonts and text
- GLFW
    - Creating windows for desktop and receiving input, 
- GLM
    - Vector/matrix/quaternion and other graphics related math
- googletest
    - Unit testing
- imgui
    - Prototype GUI
- libsndfile
    - Converting audio files into audio data useable by OpenAL
- OpenAL
    - Playing audio
- OpenXR-SDK-Source
    - Support for XR platforms
- PhysX
    - Real-time physics simulation library 
- stb
    - Loading/saving image files
- Vulkan-Hpp
    - Vulkan hpp API binding

# Load all submodules
- `git submodule update --init --recursive`

### Build
- `cmake -S . -B build`
- `cmake --build ./build/`

### Compile shaders
- `glslc -fshader-stage=vert shader.vert -g -o vert.spv`
- `glslc -fshader-stage=frag shader.frag -g -o frag.spv`

This library can be added to a cmake as a subdirectory:

```cmake
# If Building for XR
set(CLAY_PLATFORM_XR ON CACHE BOOL "Set Platform to VR" FORCE) 
# If Building for Desktop (Windows or linux)
set(CLAY_PLATFORM_DESKTOP ON CACHE BOOL "Set Platform to Desktop" FORCE) 
 # If Building for Android Mobile
set(CLAY_PLATFORM_ANDROID ON CACHE BOOL "Set Platform to Android" FORCE)

# Add ClayEngine
add_subdirectory(
    ${CMAKE_SOURCE_DIR}/thirdparty/ClayEngineVK
    ${CMAKE_BINARY_DIR}/thirdparty/ClayEngineVK
)
```

### Demos

- [Window/Linux Desktop Demo](https://github.com/VadimEngine/ClayEngineVKDemo)

- [Meta Quest XR Demo](https://github.com/VadimEngine/ClayEngineVKDemoXR)

- [Android Mobile Demo](https://github.com/VadimEngine/ClayEngineVKMobile)