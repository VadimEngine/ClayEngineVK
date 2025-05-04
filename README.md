# Clay Engine with Vulkan

Static cross-platform game engine library with support for Desktop and Oculus VR applications.

# Load all submodules
- `git submodule update --init --recursive`

### Build
- `cmake -S . -B build`
- `cmake --build ./build/`

### Run
- `./build/Debug/Sandbox.exe`

### Compile shader
- `C:/VulkanSDK/1.3.296.0/Bin/glslc.exe shader.vert -g -o vert.spv`
- `C:/VulkanSDK/1.3.296.0/Bin/glslc.exe shader.frag -g -o frag.spv`