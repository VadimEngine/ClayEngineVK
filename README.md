# Clay Engine with Vulkan

Static C++/Vulkan cross-platform game engine library with support for Desktop and Oculus XR applications. 

### Third Party Libraries
All the libraries submodules are included in `./thirdparty`:

- assimp
    - Loading 3d models and converting them to renderable components 
- freetype
    - loading and rendering fonts and text
- GLFW
    - Creating windows for desktop and receiving input, 
- GLM
    - Vector/matrix/quaternion and other graphics related math
- imgui
    - Prototype GUI
- OpenAL
    - Playing audio
- sndfile
    - Converting audio files into audio data useable by OpenAL
- stb
    - Loading/saving image files
- OpenXR-SDK-Source
    - Support for XR platforms

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
set(CLAY_PLATFORM_XR ON CACHE BOOL "Set Platform to VR" FORCE) # If Building for XR

set(CLAY_PLATFORM_DESKTOP ON CACHE BOOL "Set Platform to Desktop" FORCE) # If Building for Desktop (Windows or linux)

# Add ClayEngine
add_subdirectory(
    ${CMAKE_SOURCE_DIR}/thirdparty/ClayEngineVK
    ${CMAKE_BINARY_DIR}/thirdparty/ClayEngineVK
)
```

### Demos

- [ Desktop Demo:](https://github.com/VadimEngine/ClayEngineVKDemo)

- [XR Meta Quest Demo](https://github.com/VadimEngine/ClayEngineVKDemoXR)