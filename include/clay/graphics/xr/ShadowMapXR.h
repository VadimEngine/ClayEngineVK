#pragma once
#ifdef CLAY_PLATFORM_XR
// clay
#include "clay/graphics/common/BaseGraphicsContext.h"

namespace clay {

// A depth-only shadow map for XR scenes (the XR counterpart of GraphicsContextDesktop's
// shadow resources): a square depth image, the render pass that fills it, and the sampler
// lit shaders read it with (clamped to a white border, so anything outside the light's
// view reads as lit). Render it from BaseScene::renderShadowPass(), which AppXR calls on
// the frame's command buffer before the main (multiview) pass:
//     shadowMap.begin(cmd);
//     entityManager.renderShadowCasters(cmd, casterMaterial, lightViewProj);
//     shadowMap.end(cmd);   // leaves it shader-readable for the main pass
// Caster pipelines are built against getRenderPass() with no color attachments.
class ShadowMapXR {
public:
    ShadowMapXR() = default;
    ~ShadowMapXR();

    ShadowMapXR(const ShadowMapXR&) = delete;
    ShadowMapXR& operator=(const ShadowMapXR&) = delete;

    void create(BaseGraphicsContext& graphics, uint32_t size);
    void destroy();

    void begin(vk::CommandBuffer cmd);
    void end(vk::CommandBuffer cmd);

    vk::RenderPass getRenderPass() const { return mRenderPass_; }
    vk::ImageView getImageView() const { return mImageView_; }
    vk::Sampler getSampler() const { return mSampler_; }
    uint32_t getSize() const { return mSize_; }

private:
    BaseGraphicsContext* mpGraphics_ = nullptr;
    uint32_t mSize_ = 0;
    vk::Format mFormat_ = vk::Format::eD32Sfloat;
    vk::Image mImage_{};
    vk::DeviceMemory mMemory_{};
    vk::ImageView mImageView_{};
    vk::Sampler mSampler_{};
    vk::RenderPass mRenderPass_{};
    vk::Framebuffer mFramebuffer_{};
};

} // namespace clay

#endif // CLAY_PLATFORM_XR
