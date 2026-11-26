#pragma once
#ifdef CLAY_PLATFORM_XR
// standard lib
#include <array>
// clay
#include "clay/graphics/common/BaseGraphicsContext.h"

namespace clay {

// An offscreen color image that a 2D GUI (ImGui, RmlUi) is drawn into each frame and that
// the 3D scene then shows on an in-world panel. Owns the image (sRGB RGBA8, with a full mip
// chain so the panel minifies smoothly instead of shimmering), its render pass and
// framebuffer. Per frame, on the frame's command buffer:
//     target.begin(cmd);   // clears mip 0 and starts the render pass
//     ...draw the GUI...   // (pipelines built against target.getRenderPass())
//     target.end(cmd);     // ends the pass, rebuilds the mips, leaves it shader-readable
class PanelRenderTargetXR {
public:
    PanelRenderTargetXR() = default;
    ~PanelRenderTargetXR();

    PanelRenderTargetXR(const PanelRenderTargetXR&) = delete;
    PanelRenderTargetXR& operator=(const PanelRenderTargetXR&) = delete;

    // clearColor is what an empty pixel holds (premultiplied alpha: {0,0,0,0} = transparent).
    void create(BaseGraphicsContext& graphics, uint32_t width, uint32_t height,
                const std::array<float, 4>& clearColor = {0.0f, 0.0f, 0.0f, 1.0f});
    void destroy();
    bool isCreated() const { return mImage_ != vk::Image{}; }

    void begin(vk::CommandBuffer cmd);
    void end(vk::CommandBuffer cmd);

    // All mip levels, for sampling the panel in the scene (with a mipmapped sampler).
    vk::ImageView getSampledView() const { return mSampledView_; }
    vk::RenderPass getRenderPass() const { return mRenderPass_; }
    uint32_t getWidth() const { return mWidth_; }
    uint32_t getHeight() const { return mHeight_; }

private:
    void recordMipChain(vk::CommandBuffer cmd);

    BaseGraphicsContext* mpGraphics_ = nullptr;
    uint32_t mWidth_ = 0;
    uint32_t mHeight_ = 0;
    uint32_t mMipLevels_ = 1;
    bool mLinearBlit_ = true;
    std::array<float, 4> mClearColor_{0.0f, 0.0f, 0.0f, 1.0f};

    vk::Image mImage_{};
    vk::DeviceMemory mMemory_{};
    vk::ImageView mSampledView_{};
    vk::ImageView mAttachmentView_{}; // mip 0 only (framebuffer attachments are single-level)
    vk::RenderPass mRenderPass_{};
    vk::Framebuffer mFramebuffer_{};
};

} // namespace clay

#endif // CLAY_PLATFORM_XR
