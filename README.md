# Vulkan Simple Project

# Load all submodules
- `git submodule update --init --recursive`

### Build
- `cmake -S . -B build`
- `cmake --build ./build/`

### Run
- `./build/Debug/Sandbox.exe`

### Compile shader
- `C:/VulkanSDK/1.3.296.0/Bin/glslc.exe shader.vert -o vert.spv`
- `C:/VulkanSDK/1.3.296.0/Bin/glslc.exe shader.frag -o frag .spv`