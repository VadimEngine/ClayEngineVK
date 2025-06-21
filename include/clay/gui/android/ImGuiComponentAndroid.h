#pragma once

#ifdef CLAY_PLATFORM_ANDROID
// third party
#include "imgui.h"
#include <android/log.h>
#include <android_native_app_glue.h>
// clay
#include "clay/graphics/common/BaseGraphicsContext.h"

namespace clay {

class ImGuiComponentAndroid {
public:
    static void initialize(android_app* app, BaseGraphicsContext* graphics);

    static void beginRender();

    static void endRender(VkCommandBuffer cmdBuffer);

    static void finalize();

private:
    static BaseGraphicsContext* mpGraphics_;
    static VkDescriptorPool mImguiDescriptorPool_;
};

} // namespace clay

#endif // CLAY_PLATFORM_ANDROID