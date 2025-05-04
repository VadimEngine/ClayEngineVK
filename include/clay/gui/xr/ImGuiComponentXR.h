#pragma once
#ifdef CLAY_PLATFORM_XR
// clay
#include "clay/graphics/common/IGraphicsContext.h"

// third party
#include "imgui.h"
#include "imgui_impl_android.h"
#include "imgui_impl_vulkan.h"

namespace clay {
class ImGuiComponentXR {
public:
    ImGuiComponentXR();

    virtual ~ImGuiComponentXR() = default;

    static void initialize(ANativeWindow* pWindow);

    static void deinitialize();

    static void startRender();
    virtual void buildImGui();
    static void endRender();

    void render();

    static IGraphicsContext* gGraphicsContext_;
    static VkDescriptorPool gImguiDescriptorPool_;
};
} // namespace clay

#endif // CLAY_PLATFORM_XR
