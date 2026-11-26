#pragma once
#ifdef CLAY_PLATFORM_XR
// standard lib
#include <string>
// third party
#include <android_native_app_glue.h>
// clay
#include "clay/graphics/common/BaseGraphicsContext.h"

namespace clay {

// RmlUi for XR: one view (.rml + .rcss, from the APK's assets) rendered every frame into an
// offscreen panel image that the app shows on a quad in the world (see getImageView()), the
// XR counterpart of UiComponentDesktop / UiComponentAndroid. Meant to take over from the
// ImGui panel: same idea, but real layout, styling and fonts.
//
// The panel's pixels are premultiplied alpha on a transparent background: draw the quad
// with a premultiplied blend (src = ONE, dst = ONE_MINUS_SRC_ALPHA).
//
// AppXR renders it (render()) each frame after the scene's update; the app only loads a
// view, feeds it input and values, and reads back commands. Everything is a no-op (and
// isAvailable() is false) unless the engine is built with CLAY_ENABLE_RMLUI.
class UiComponentXR {
public:
    // uiRoot: asset folder the views, style sheets and fonts (*.ttf, *.otf) load from.
    // width x height: the panel image size in pixels (the view's layout size).
    static bool initialize(android_app* app, BaseGraphicsContext* graphics, const char* uiRoot,
                           uint32_t width, uint32_t height);
    static bool isAvailable();
    static void finalize();

    // Scale of the RCSS "dp" unit in panel pixels (1 = 1 px).
    static void setDpRatio(float ratio);

    static bool loadView(const char* viewFile);   // under uiRoot
    static void unloadView();
    static bool hasView();

    // The panel image (all mip levels, shader-readable once a frame has rendered).
    static vk::ImageView getImageView();
    static uint32_t getWidth();
    static uint32_t getHeight();

    // Called by AppXR once per frame on the frame's command buffer, outside any render pass:
    // updates layout, draws the view into the panel image and rebuilds its mips.
    static void render(vk::CommandBuffer cmd);

    // false: the panel isn't on show, so render() skips the work (the image keeps its last
    // frame). Turn it back on before the frame the panel reappears in.
    static void setActive(bool active);
    static bool isActive();

    // Pointer input in panel pixels (top-left origin). Return whether the pointer is on UI.
    static bool onMouseMove(int x, int y);
    static bool onMouseButton(int x, int y, int button, bool down); // 0=L,1=R,2=M
    static void onMouseLeave();
    static bool scrollAt(int x, int y, float pixels);

    // id of a <button> clicked since the last call (FIFO); false when none.
    static bool popCommand(std::string& out);

    // Values in the loaded view, by element id (see UiBridge.h).
    static void setText(const char* id, const std::string& text);
    // Replace an element's content with RML markup (lists built from app data).
    static void setInnerRml(const char* id, const std::string& rml);
    static bool getToggle(const char* id, bool& out);
    static void setToggle(const char* id, bool value);
    static bool getRangeValue(const char* id, float& out);
    static void setRangeValue(const char* id, float value);
    static void setProperty(const char* id, const char* property, const std::string& value);
    static void setVisible(const char* id, bool visible);
    static void setEnabled(const char* id, bool enabled);
};

} // namespace clay

#endif // CLAY_PLATFORM_XR
