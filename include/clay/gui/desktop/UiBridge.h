#pragma once
// The engine's UI bridge: the game's permanent interface (menus, HUD), written as RmlUi
// documents (.rml + .rcss) and driven by element id. UiComponentDesktop is the engine-side
// wrapper most code uses; these are the plain functions behind it, implemented by
// src/clay/gui/desktop/RmlUiRenderBridge.cpp.
//
// All functions are no-ops unless the engine is built with CLAY_ENABLE_RMLUI.
#include <cstdint>
#include <functional>
#include <string>
#include <vector>
#include "clay/utils/common/Utils.h"

namespace clay {
class BaseGraphicsContext;

namespace uibridge {

// Boot RmlUi on this graphics context. uiRoot is the game's UI folder: views (.rml), style
// sheets (.rcss) and fonts load from it; images are named relative to its parent (the
// resource root). Returns false if it could not start (or RmlUi is not built in).
bool init(BaseGraphicsContext* graphics, const char* uiRoot);

// Platform hooks, set before init(). The image loader opens <img> sources (default on
// desktop: files via stb); the file lister names the files in a folder, for the fonts
// (default: std::filesystem). Android sets both to read the APK's assets.
void setImageLoader(std::function<utils::ImageData(const std::string&)> loader);
void setFileLister(std::function<std::vector<std::string>(const std::string&)> lister);
// A view's RML text passes through this before it is parsed (nullptr = as written): the
// place to translate it. Relative paths in the view still resolve from its file.
void setViewTextFilter(std::function<void(std::string&)> filter);

bool isInitialized();

// Scale of the RmlUi "dp" unit (density-independent pixels): 1 = 1 px. "px" stays physical.
void setDpRatio(float ratio);

// Load (replace) the root view from an .rml file under the UI root. Returns success.
bool loadView(const char* viewFile, uint32_t width, uint32_t height);
void unloadView();
bool hasView();

// Per-frame: update layout/clock, then the two render phases around the main pass.
void update(double timeSeconds, uint32_t width, uint32_t height);
void setCommandBuffer(void* commandBuffer);                 // call once, after cmd begin
void renderOffscreen();                                     // BEFORE the main render pass
void renderOnscreen(void* renderPass, uint32_t sampleCount); // INSIDE the main render pass

// Input forwarding (returns whether the pointer is over the UI).
bool mouseMove(int x, int y);
bool mouseButton(int x, int y, int button, bool down);      // button: 0=left,1=right,2=middle
bool mouseWheel(int x, int y, int delta);
// Scrolls the scrollable element under (x, y) down by `pixels` (negative = up), exactly and
// at once: finger-drag scrolling. Returns false if nothing there scrolled (none, or at its end).
bool scrollAt(int x, int y, float pixels);

// Optional UI SFX hooks. hover() is called once when the cursor enters a new named
// Button; press() on a left-button-down over a named Button. Pass nullptr to disable.
void setUiSoundCallbacks(void (*hover)(), void (*press)());

// Named element that the user clicked since the last call (FIFO). Returns false when
// the queue is empty. Lets the scene react to menu buttons without UI library types.
bool popCommand(char* out, int cap);

// Value interop: find an element by id in the current view and get/set its value.
// getters return false if the view/element is absent or the wrong type.
void setText(const char* name, const char* utf8);           // TextBlock.Text
bool getToggle(const char* name, bool* out);                // ToggleButton/CheckBox.IsChecked
void setToggle(const char* name, bool value);
bool getRangeValue(const char* name, float* out);           // Slider/ProgressBar.Value
void setRangeValue(const char* name, float value);

// Dynamic list: rebuild clickable rows inside a named Panel (e.g. a StackPanel).
// Each row is a Button named cmdName (clicks arrive via popCommand), with an optional
// colour swatch + text; selected rows get a highlighted background.
void clearList(const char* panelName);
void addListRow(const char* panelName, const char* cmdName, const char* text,
                bool hasColor, float r, float g, float b, bool selected);
// A crafting row: output icon (atlas col,row) + name, "<-", then each ingredient's
// icon + count. Greyed when !craftable. Icons are 128px tiles in `atlasFile`.
void addCraftRow(const char* panelName, const char* cmdName, const char* atlasFile,
                 int outCol, int outRow, const char* outName,
                 const int* ingCols, const int* ingRows, const int* ingCounts, int ingN,
                 bool craftable);
// A list row with an icon: one 128px tile (col,row) of `atlasFile`, then text.
void addIconRow(const char* panelName, const char* cmdName, const char* atlasFile,
                int col, int row, const char* text);

// Editable TextBox interop.
bool getEditText(const char* name, char* out, int cap);     // TextBox.Text
void setEditText(const char* name, const char* text);
void setElementColor(const char* name, float r, float g, float b);  // Border.Background
// Set any RCSS property on a named element, e.g. ("Title", "opacity", "0.5"). An empty
// value removes the inline property again.
void setProperty(const char* name, const char* property, const char* value);
// Show one atlas tile (pixel rect) in a named Image via a CroppedBitmap; w<=0 clears.
void setIconTile(const char* imageName, const char* atlasFile, int x, int y, int w, int h);
void setVisible(const char* name, bool visible);   // Visibility Visible/Collapsed
void setEnabled(const char* name, bool enabled);   // IsEnabled
void setMargin(const char* name, float l, float t, float r, float b);  // position a popup

// App-updated dynamic image: bind a GPU texture to a named <Image> once, then push a
// tightly-packed RGBA8 buffer (width*height*4 bytes) each frame. The upload happens at
// a safe point inside renderOffscreen(). Used e.g. for a CPU-rendered minimap.
bool createDynamicImage(const char* name, uint32_t width, uint32_t height);
void updateDynamicImage(const char* name, const void* rgba, uint32_t width, uint32_t height);
// Name of the nearest named Button under (x,y), or false. For slot tooltips / menus.
bool hitTestButton(int x, int y, char* out, int cap);

// Keyboard forwarding (GLFW key codes; printable chars go through charInput).
bool keyDown(int glfwKey);
bool keyUp(int glfwKey);
bool charInput(unsigned int codepoint);
void focusElement(const char* name);                        // activate view + focus element

void shutdown();

} } // namespace clay::uibridge
