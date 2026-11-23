#pragma once
#ifdef CLAY_PLATFORM_DESKTOP
// clay
#include "clay/gui/desktop/Window.h"
#include "clay/graphics/common/BaseGraphicsContext.h"
#include <functional>
#include <string>

// The game's permanent UI (menus, HUD): RmlUi documents (.rml + .rcss) rendered by the
// engine's own Vulkan renderer, while ImGui stays for debug overlays. This wrapper owns the
// UI subsystem and one root view, and exposes value interop by element id.
//
// Everything is a no-op unless the engine is built with CLAY_ENABLE_RMLUI.

namespace clay {

class UiComponentDesktop {
public:
    // Boot RmlUi + create the Vulkan render device from this graphics context.
    // uiRoot is the folder RML/fonts load from (e.g. the game's res/ui).
    static void initialize(Window& window, BaseGraphicsContext* graphics, const char* uiRoot);
    // True only when built with CLAY_ENABLE_NOESIS and successfully booted.
    static bool isAvailable();
    // How big the UI's "dp" unit is in pixels (e.g. screen height / design height).
    static void setDpRatio(float ratio);

    static bool loadView(const char* viewFile);   // under uiRoot
    static void unloadView();
    static bool hasView();

    // Per-frame: advance clock/layout (sized to the swapchain).
    static void update(double timeSeconds);
    // Offscreen phase — call BEFORE the main render pass begins (records glyph/mask
    // targets onto `cmdBuffer`).
    static void renderOffscreen(vk::CommandBuffer cmdBuffer);
    // Onscreen phase — call INSIDE the main render pass (after the 3D scene).
    static void render(vk::CommandBuffer cmdBuffer);

    // Input forwarding (returns whether RmlUi consumed it).
    static bool onMouseMove(int x, int y);
    static bool onMouseButton(int x, int y, int button, bool down); // 0=L,1=R,2=M
    static bool onMouseWheel(int x, int y, int delta);
    // Scrolls the scrollable element under (x, y) by `pixels` (positive = down): finger scrolling.
    static bool scrollAt(int x, int y, float pixels);
    // A view's RML text passes through this before it is parsed (translation); see UiBridge.h.
    static void setViewTextFilter(std::function<void(std::string&)> filter);

    // Register UI SFX hooks: hover() fires when the cursor enters a new named Button,
    // press() on a left-button-down over one. Pass nullptr to disable.
    static void setUiSoundCallbacks(void (*hover)(), void (*press)());

    // Pops the name of a UI element clicked since the last call (empty if none).
    static bool popCommand(std::string& out);

    // Value interop with the loaded view, by element id (see UiBridge).
    static void setText(const char* name, const std::string& text);
    static bool getToggle(const char* name, bool& out);  // CheckBox/ToggleButton
    static void setToggle(const char* name, bool value);
    static bool getRangeValue(const char* name, float& out);  // Slider/ProgressBar
    static void setRangeValue(const char* name, float value);

    // Dynamic list rows inside a named Panel (see UiBridge).
    static void clearList(const char* panelName);
    static void addListRow(const char* panelName, const char* cmdName, const char* text,
                           bool hasColor, float r, float g, float b, bool selected);
    // Crafting row with icons: output tile (col,row) + name, then each ingredient's
    // tile + count; greyed when !craftable. Tiles index a 128px-tile atlas file.
    static void addCraftRow(const char* panelName, const char* cmdName, const char* atlasFile,
                            int outCol, int outRow, const char* outName,
                            const int* ingCols, const int* ingRows, const int* ingCounts,
                            int ingN, bool craftable);
    // List row with an icon: one 128px tile (col,row) of the atlas file, then text.
    static void addIconRow(const char* panelName, const char* cmdName, const char* atlasFile,
                           int col, int row, const char* text);

    // Editable TextBox interop + keyboard forwarding.
    static bool getEditText(const char* name, std::string& out);
    static void setEditText(const char* name, const std::string& text);
    static void setElementColor(const char* name, float r, float g, float b);
    // Any RCSS property by element id, e.g. setProperty("Title", "opacity", "0.5").
    static void setProperty(const char* name, const char* property, const std::string& value);
    static void setIconTile(const char* imageName, const char* atlasFile,
                            int x, int y, int w, int h);
    static void setVisible(const char* name, bool visible);
    static void setEnabled(const char* name, bool enabled);
    static void setMargin(const char* name, float l, float t, float r, float b);
    // App-updated dynamic image for a named <Image> (e.g. a CPU-rendered minimap):
    // create once, then push a tightly-packed RGBA8 buffer (w*h*4 bytes) each frame.
    static bool createDynamicImage(const char* name, uint32_t width, uint32_t height);
    static void updateDynamicImage(const char* name, const void* rgba,
                                   uint32_t width, uint32_t height);
    static bool hitTestButton(int x, int y, std::string& out);
    static bool keyDown(int glfwKey);
    static bool keyUp(int glfwKey);
    static bool charInput(unsigned int codepoint);
    static void focusElement(const char* name);

    static void finalize();

private:
    static BaseGraphicsContext* mpGraphics_;
};

} // namespace clay

#endif // CLAY_PLATFORM_DESKTOP
