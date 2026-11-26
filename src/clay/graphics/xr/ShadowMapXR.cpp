#ifdef CLAY_PLATFORM_XR

// class
#include "clay/graphics/xr/ShadowMapXR.h"
// standard lib
#include <array>

namespace clay {

ShadowMapXR::~ShadowMapXR() {
    destroy();
}

void ShadowMapXR::create(BaseGraphicsContext& graphics, uint32_t size) {
    destroy();
    mpGraphics_ = &graphics;
    mSize_ = size;
    vk::Device device = graphics.getDevice();

    // A depth format that can also be sampled.
    mFormat_ = vk::Format::eD32Sfloat;
    for (vk::Format candidate : {vk::Format::eD32Sfloat, vk::Format::eD16Unorm}) {
        const auto features = graphics.mPhysicalDevice_.getFormatProperties(candidate).optimalTilingFeatures;
        if ((features & vk::FormatFeatureFlagBits::eDepthStencilAttachment) &&
            (features & vk::FormatFeatureFlagBits::eSampledImage)) {
            mFormat_ = candidate;
            break;
        }
    }

    graphics.createImage(
        size, size, 1,
        vk::SampleCountFlagBits::e1,
        mFormat_,
        vk::ImageTiling::eOptimal,
        vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eSampled,
        vk::MemoryPropertyFlagBits::eDeviceLocal,
        mImage_, mMemory_);
    mImageView_ = graphics.createImageView(mImage_, mFormat_, vk::ImageAspectFlagBits::eDepth, 1);

    vk::SamplerCreateInfo samplerInfo{
        .magFilter = vk::Filter::eLinear,
        .minFilter = vk::Filter::eLinear,
        .mipmapMode = vk::SamplerMipmapMode::eNearest,
        .addressModeU = vk::SamplerAddressMode::eClampToBorder,
        .addressModeV = vk::SamplerAddressMode::eClampToBorder,
        .addressModeW = vk::SamplerAddressMode::eClampToBorder,
        .borderColor = vk::BorderColor::eFloatOpaqueWhite,
    };
    mSampler_ = device.createSampler(samplerInfo);

    // Depth-only pass: cleared, stored, left shader-readable.
    vk::AttachmentDescription depthAttachment{
        .format = mFormat_,
        .samples = vk::SampleCountFlagBits::e1,
        .loadOp = vk::AttachmentLoadOp::eClear,
        .storeOp = vk::AttachmentStoreOp::eStore,
        .stencilLoadOp = vk::AttachmentLoadOp::eDontCare,
        .stencilStoreOp = vk::AttachmentStoreOp::eDontCare,
        .initialLayout = vk::ImageLayout::eUndefined,
        .finalLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
    };
    vk::AttachmentReference depthRef{0, vk::ImageLayout::eDepthStencilAttachmentOptimal};
    vk::SubpassDescription subpass{
        .pipelineBindPoint = vk::PipelineBindPoint::eGraphics,
        .colorAttachmentCount = 0,
        .pDepthStencilAttachment = &depthRef,
    };
    // Previous frame's sampling -> this frame's depth writes, and depth writes -> sampling.
    std::array<vk::SubpassDependency, 2> deps{};
    deps[0] = vk::SubpassDependency{
        .srcSubpass = VK_SUBPASS_EXTERNAL,
        .dstSubpass = 0,
        .srcStageMask = vk::PipelineStageFlagBits::eFragmentShader,
        .dstStageMask = vk::PipelineStageFlagBits::eEarlyFragmentTests,
        .srcAccessMask = vk::AccessFlagBits::eShaderRead,
        .dstAccessMask = vk::AccessFlagBits::eDepthStencilAttachmentWrite,
        .dependencyFlags = vk::DependencyFlagBits::eByRegion,
    };
    deps[1] = vk::SubpassDependency{
        .srcSubpass = 0,
        .dstSubpass = VK_SUBPASS_EXTERNAL,
        .srcStageMask = vk::PipelineStageFlagBits::eLateFragmentTests,
        .dstStageMask = vk::PipelineStageFlagBits::eFragmentShader,
        .srcAccessMask = vk::AccessFlagBits::eDepthStencilAttachmentWrite,
        .dstAccessMask = vk::AccessFlagBits::eShaderRead,
        .dependencyFlags = vk::DependencyFlagBits::eByRegion,
    };
    vk::RenderPassCreateInfo rpInfo{
        .attachmentCount = 1,
        .pAttachments = &depthAttachment,
        .subpassCount = 1,
        .pSubpasses = &subpass,
        .dependencyCount = static_cast<uint32_t>(deps.size()),
        .pDependencies = deps.data(),
    };
    mRenderPass_ = device.createRenderPass(rpInfo);

    vk::FramebufferCreateInfo fbInfo{
        .renderPass = mRenderPass_,
        .attachmentCount = 1,
        .pAttachments = &mImageView_,
        .width = size,
        .height = size,
        .layers = 1,
    };
    mFramebuffer_ = device.createFramebuffer(fbInfo);
}

void ShadowMapXR::destroy() {
    if (!mpGraphics_) return;
    vk::Device device = mpGraphics_->getDevice();
    device.waitIdle();
    if (mFramebuffer_) device.destroyFramebuffer(mFramebuffer_, nullptr);
    if (mRenderPass_) device.destroyRenderPass(mRenderPass_, nullptr);
    if (mSampler_) device.destroySampler(mSampler_, nullptr);
    if (mImageView_) device.destroyImageView(mImageView_, nullptr);
    if (mImage_) device.destroyImage(mImage_, nullptr);
    if (mMemory_) device.freeMemory(mMemory_, nullptr);
    mFramebuffer_ = nullptr;
    mRenderPass_ = nullptr;
    mSampler_ = nullptr;
    mImageView_ = nullptr;
    mImage_ = nullptr;
    mMemory_ = nullptr;
    mpGraphics_ = nullptr;
}

void ShadowMapXR::begin(vk::CommandBuffer cmd) {
    vk::ClearValue clear{};
    clear.depthStencil = vk::ClearDepthStencilValue{.depth = 1.0f, .stencil = 0};
    vk::RenderPassBeginInfo info{
        .renderPass = mRenderPass_,
        .framebuffer = mFramebuffer_,
        .renderArea = {.offset = {0, 0}, .extent = {mSize_, mSize_}},
        .clearValueCount = 1,
        .pClearValues = &clear,
    };
    cmd.beginRenderPass(info, vk::SubpassContents::eInline);
    cmd.setViewport(0, vk::Viewport{0.0f, 0.0f, static_cast<float>(mSize_), static_cast<float>(mSize_), 0.0f, 1.0f});
    cmd.setScissor(0, vk::Rect2D{{0, 0}, {mSize_, mSize_}});
}

void ShadowMapXR::end(vk::CommandBuffer cmd) {
    cmd.endRenderPass();
}

} // namespace clay

#endif // CLAY_PLATFORM_XR
