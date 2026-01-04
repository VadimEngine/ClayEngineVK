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

### Build with Tests
ClayEngineVK includes a comprehensive test suite organized by platform. Tests are **not built by default** to keep the library lightweight for end users.

#### Running Tests on Windows/Linux (Desktop)
```bash
# Configure with tests enabled
cmake -S . -B build -DCLAY_PLATFORM_DESKTOP=ON -DCLAYENGINE_BUILD_TESTS=ON

# Build the tests
cmake --build build

# Run all tests
ctest --test-dir build --output-on-failure

# Run only desktop tests
ctest --test-dir build -L desktop --output-on-failure

# Run only common/cross-platform tests
ctest --test-dir build -L common --output-on-failure
```

#### Running Tests on Android
```bash
# Configure with tests enabled for Android
cmake -S . -B build -DCLAY_PLATFORM_ANDROID=ON -DCLAYENGINE_BUILD_TESTS=ON

# Build
cmake --build build

# Run Android tests
ctest --test-dir build -L android --output-on-failure
```

#### Running Tests on XR (Oculus Quest)
```bash
# Configure with tests enabled for XR
cmake -S . -B build -DCLAY_PLATFORM_XR=ON -DCLAYENGINE_BUILD_TESTS=ON

# Build
cmake --build build

# Run XR tests
ctest --test-dir build -L xr --output-on-failure
```

#### Test Organization
Tests are organized by platform in the `tests/` directory:
```
tests/
├── unit/
│   ├── desktop/       # Desktop-specific tests (Windows, Linux)
│   ├── android/       # Android-specific tests
│   ├── xr/           # XR platform tests (Oculus Quest)
│   ├── ResourcesTest.cpp  # Cross-platform resource tests
│   └── ECSTest.cpp        # Cross-platform ECS tests
├── integration/       # Integration tests (future)
└── TestHelpers.h      # Common test utilities
```

#### Available Test Executables
- `ClayEngineVK_DesktopTests` - Desktop platform tests (Windows/Linux)
- `ClayEngineVK_AndroidTests` - Android platform tests
- `ClayEngineVK_XRTests` - XR platform tests
- `ClayEngineVK_CommonTests` - Cross-platform tests (Resources, ECS, etc.)

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