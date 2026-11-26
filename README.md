# Clay Engine with Vulkan

ClayEngineVK is a C++20 game engine built on Vulkan. It compiles as a static library that a game adds to its own CMake project, and it runs on Windows, Linux, Android and Meta Quest (OpenXR).

## What's in it

| Area | What it provides | Headers |
|---|---|---|
| Application | App and scene base classes, the main loop, input, and a typed resource store with handles | `clay/application` |
| Graphics | Vulkan context per platform, meshes, materials, pipelines, textures, cameras, fonts, skybox, tilemaps, 2D sprite animation, skeletal animation | `clay/graphics` |
| ECS | Entities and components (transform, model, sprite, text, 2D and 3D animation, colliders, physics bodies) with render, collision and physics systems | `clay/ecs` |
| Physics | 2D bodies and collision events in the ECS, and PhysX rigid bodies for 3D | `clay/ecs/systems` |
| Audio | OpenAL playback: loaded sounds, sources, music. Reads WAV and other uncompressed formats, and Ogg Vorbis for compressed music | `clay/audio` |
| UI | Dear ImGui for debug tools; optional RmlUi (HTML/CSS-like views) for game menus and HUDs | `clay/gui` |
| Utilities | Logging, file and image loading, a frame profiler | `clay/utils` |

Each area has a `common` part shared by every platform and a `desktop`, `android` or `xr` part where the platforms differ.

The renderer does frustum culling, sorts opaque draws by pipeline, material and mesh, instances repeated meshes on the GPU, and can render shadow casters into a shadow map.

## Getting the source

The third-party libraries are git submodules under `thirdparty/`:

```bash
git submodule update --init --recursive
```

You also need the [Vulkan SDK](https://vulkan.lunarg.com/) (the build uses `find_package(Vulkan)`, and `glslc` from the SDK compiles shaders).

For Android and Quest builds, PhysX's Android build files need the fixes in `patches/physx_fixes.patch`. Apply it once from the repository root:

```bash
git apply patches/physx_fixes.patch
```

## Using it in a game

Pick one platform, set any options, then add the engine as a subdirectory and link it:

```cmake
# Exactly one of these:
set(CLAY_PLATFORM_DESKTOP ON CACHE BOOL "Windows or Linux" FORCE)
# set(CLAY_PLATFORM_ANDROID ON CACHE BOOL "Android phones and tablets" FORCE)
# set(CLAY_PLATFORM_XR ON CACHE BOOL "Meta Quest (OpenXR)" FORCE)

# Optional: game UI on RmlUi (desktop and Android)
set(CLAY_ENABLE_RMLUI ON CACHE BOOL "Game UI on RmlUi" FORCE)

add_subdirectory(
    ${CMAKE_SOURCE_DIR}/thirdparty/ClayEngineVK
    ${CMAKE_BINARY_DIR}/thirdparty/ClayEngineVK
)
target_link_libraries(MyGame PRIVATE ClayEngineVK)
```

| Option | Default | Meaning |
|---|---|---|
| `CLAY_PLATFORM_DESKTOP` / `CLAY_PLATFORM_ANDROID` / `CLAY_PLATFORM_XR` | none | The platform to build for. Set exactly one. |
| `CLAY_ENABLE_RMLUI` | `OFF` | Builds the RmlUi-based game UI. When off, the UI functions compile to no-ops. |
| `CLAYENGINE_BUILD_TESTS` | `OFF` | Builds the test suite (see below). |

On Android and Quest the game is a native library loaded by a `NativeActivity`; the demos below show the Gradle project around it.

### Building the library alone

```bash
cmake -S . -B build -DCLAY_PLATFORM_DESKTOP=ON
cmake --build build
```

## Game UI with RmlUi

With `CLAY_ENABLE_RMLUI=ON`, a game writes its menus and HUD as RML views with RCSS stylesheets and drives them through a small static API: `clay::UiComponentDesktop` on desktop and `clay::UiComponentAndroid` on Android, which have the same functions.

- **Views:** `loadView("Menu.rml")` shows a view from the game's UI folder. Fonts (`.ttf`, `.otf`) in that folder are loaded automatically, and each also serves as a fallback for characters another font lacks.
- **Input out:** a click on an element with an id arrives as a command, read with `popCommand`.
- **State in:** `setText`, `setVisible`, `setEnabled`, `setProperty`, and range and toggle values address elements by id.
- **Scaling:** `setDpRatio` sets the size of the `dp` unit, so one layout fits any screen.
- **Translation:** `setViewTextFilter` passes a view's text through a function before it is parsed.
- **Touch:** `scrollAt` scrolls the list under a point by a number of pixels, for drag scrolling.

The engine renders RmlUi with its own Vulkan backend. Dear ImGui stays available alongside it for debug overlays.

## Development switches (desktop)

These environment variables are read at start-up. They exist for scripted runs and profiling.

| Variable | Effect |
|---|---|
| `CLAY_WINDOW_HIDDEN=1` | Runs without showing a window. Frames still render, and the window is borderless so it can be larger than the screen. |
| `CLAY_CAPTURE=<file.png>` | Saves a frame to the file and quits. |
| `CLAY_CAPTURE_AFTER=<seconds>` | When to take it (default 5). |
| `CLAY_CAPTURE_COUNT=<n>`, `CLAY_CAPTURE_EVERY=<seconds>` | Takes `n` frames that far apart, saved as `file_0.png`, `file_1.png`, and so on. |
| `CLAY_PROFILE=1` | Turns on the frame profiler. |
| `CLAY_NO_INSTANCING=1` | Draws every object separately, to compare against GPU instancing. |

## Shaders

Shaders are GLSL compiled to SPIR-V ahead of time:

```bash
glslc -fshader-stage=vert shader.vert -g -o vert.spv
glslc -fshader-stage=frag shader.frag -g -o frag.spv
```

## Tests

Tests are off by default so that games don't build them. They are grouped by platform and labelled for `ctest`.

```bash
cmake -S . -B build -DCLAY_PLATFORM_DESKTOP=ON -DCLAYENGINE_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure            # everything built for this platform
ctest --test-dir build -L desktop --output-on-failure # one label: desktop, android, xr or common
```

For Android or Quest, configure with `-DCLAY_PLATFORM_ANDROID=ON` or `-DCLAY_PLATFORM_XR=ON` in place of the desktop option.

```
tests/
├── unit/
│   ├── common/            # cross-platform, no graphics device needed:
│   │                      # camera, 2D collision, ECS, frustum, profiler, resources, utils
│   ├── desktop/           # Windows and Linux
│   ├── android/
│   └── xr/                # Meta Quest
├── TestHelpers.h
└── README.md              # more detail
```

| Executable | Covers |
|---|---|
| `ClayEngineVK_DesktopTests` | Desktop window, app and input |
| `ClayEngineVK_AndroidTests` | Android app, graphics context and touch input |
| `ClayEngineVK_XRTests` | XR app, graphics context, input and tracking |
| `ClayEngineVK_CommonTests` | Camera conventions, 2D collision, ECS, frustum culling, profiler, resources, image conversion |

## Third-party libraries

All are submodules in `thirdparty/`.

| Library | Used for |
|---|---|
| assimp | Loading 3D models and animations |
| freetype | Loading fonts and rendering text |
| GLFW | Desktop windows and input |
| GLM | Vector, matrix and quaternion math |
| googletest | Unit tests |
| Dear ImGui | Debug and prototype UI |
| nlohmann/json | Reading and writing JSON |
| libsndfile | Decoding audio files for OpenAL |
| OpenAL Soft | Audio playback |
| OpenXR-SDK-Source | XR platforms |
| PhysX | 3D physics |
| RmlUi | Game UI (optional, `CLAY_ENABLE_RMLUI`) |
| stb | Loading and saving images; decoding Ogg Vorbis audio |
| Vulkan-Hpp | C++ bindings for Vulkan |

## Demos

- [Windows / Linux desktop demo](https://github.com/VadimEngine/ClayEngineVKDemo)
- [Meta Quest XR demo](https://github.com/VadimEngine/ClayEngineVKDemoXR)
- [Android mobile demo](https://github.com/VadimEngine/ClayEngineVKMobile)
