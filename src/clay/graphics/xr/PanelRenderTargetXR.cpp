#ifdef CLAY_PLATFORM_XR

// class
#include "clay/graphics/xr/PanelRenderTargetXR.h"
// standard lib
#include <algorithm>
#include <cmath>

namespace clay {

namespace {
constexpr vk::Format kFormat = vk::Format::eR8G8B8A8Srgb;
}

PanelRenderTargetXR::~PanelRenderTargetXR() {
    destroy();
}

void PanelRenderTargetXR::create(BaseGraphicsContext& graphics, uint32_t width, uint32_t height,
                                 const std::array<float, 4>& clearColor) {
    destroy();
    mpGraphics_ = &graphics;
    mWidth_ = width;
    mHeight_ = height;
    mClearColor_ = clearColor;
    vk::Device device = graphics.getDevice();

    mMipLevels_ = static_cast<uint32_t>(std::floor(std::log2(std::max(width, height)))) + 1;
    {
        const vk::FormatProperties props = graphics.mPhysicalDevice_.getFormatProperties(kFormat);
        const auto features = props.optimalTilingFeatures;
        if (!(features & vk::FormatFeatureFlagBits::eBlitSrc) || !(features & vk::FormatFeatureFlagBits::eBlitDst)) {
            mMipLevels_ = 1; // can't build the chain on the GPU: a single level
        }
        mLinearBlit_ = static_cast<bool>(features & vk::FormatFeatureFlagBits::eSampledImageFilterLinear);
    }

    // Render pass: one color attachment, cleared, left as a color attachment (end() moves it
    // on to the mip chain / shader reads).
    vk::AttachmentDescription colorAttachment{
        .format = kFormat,
        .samples = vk::SampleCountFlagBits::e1,
        .loadOp = vk::AttachmentLoadOp::eClear,
        .storeOp = vk::AttachmentStoreOp::eStore,
        .stencilLoadOp = vk::AttachmentLoadOp::eDontCare,
        .stencilStoreOp = vk::AttachmentStoreOp::eDontCare,
        .initialLayout = vk::ImageLayout::eColorAttachmentOptimal,
        .finalLayout = vk::ImageLayout::eColorAttachmentOptimal,
    };
    vk::AttachmentReference colorRef{0, vk::ImageLayout::eColorAttachmentOptimal};
    vk::SubpassDescription subpass{
        .pipelineBindPoint = vk::PipelineBindPoint::eGraphics,
        .colorAttachmentCount = 1,
        .pColorAttachments = &colorRef,
    };
    vk::SubpassDependency dependency{
        .srcSubpass = VK_SUBPASS_EXTERNAL,
        .dstSubpass = 0,
        .srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput,
        .dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput,
        .srcAccessMask = {},
        .dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite,
    };
    vk::RenderPassCreateInfo renderPassInfo{
        .attachmentCount = 1,
        .pAttachments = &colorAttachment,
        .subpassCount = 1,
        .pSubpasses = &subpass,
        .dependencyCount = 1,
        .pDependencies = &dependency,
    };
    mRenderPass_ = device.createRenderPass(renderPassInfo);

    graphics.createImage(
        width, height, mMipLevels_,
        vk::SampleCountFlagBits::e1,
        kFormat,
        vk::ImageTiling::eOptimal,
        vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled |
            vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eTransferSrc,
        vk::MemoryPropertyFlagBits::eDeviceLocal,
        mImage_,
        mMemory_
    );
    // Shader-readable from the start, so a panel drawn before the first GUI frame is valid.
    graphics.transitionImageLayout(mImage_, kFormat, vk::ImageLayout::eUndefined,
                                   vk::ImageLayout::eShaderReadOnlyOptimal, mMipLevels_);

    mSampledView_ = graphics.createImageView(mImage_, kFormat, vk::ImageAspectFlagBits::eColor, mMipLevels_);
    mAttachmentView_ = graphics.createImageView(mImage_, kFormat, vk::ImageAspectFlagBits::eColor, 1);

    vk::FramebufferCreateInfo framebufferInfo{
        .renderPass = mRenderPass_,
        .attachmentCount = 1,
        .pAttachments = &mAttachmentView_,
        .width = width,
        .height = height,
        .layers = 1,
    };
    mFramebuffer_ = device.createFramebuffer(framebufferInfo);
}

void PanelRenderTargetXR::destroy() {
    if (!mpGraphics_) return;
    vk::Device device = mpGraphics_->getDevice();
    device.waitIdle();
    if (mFramebuffer_) device.destroyFramebuffer(mFramebuffer_, nullptr);
    if (mAttachmentView_) device.destroyImageView(mAttachmentView_, nullptr);
    if (mSampledView_) device.destroyImageView(mSampledView_, nullptr);
    if (mImage_) device.destroyImage(mImage_, nullptr);
    if (mMemory_) device.freeMemory(mMemory_, nullptr);
    if (mRenderPass_) device.destroyRenderPass(mRenderPass_, nullptr);
    mFramebuffer_ = nullptr;
    mAttachmentView_ = nullptr;
    mSampledView_ = nullptr;
    mImage_ = nullptr;
    mMemory_ = nullptr;
    mRenderPass_ = nullptr;
    mpGraphics_ = nullptr;
}

void PanelRenderTargetXR::begin(vk::CommandBuffer cmd) {
    // Mip 0 back to a color attachment (its old contents are cleared anyway).
    vk::ImageMemoryBarrier toAttachment{
        .srcAccessMask = {},
        .dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite,
        .oldLayout = vk::ImageLayout::eUndefined,
        .newLayout = vk::ImageLayout::eColorAttachmentOptimal,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = mImage_,
        .subresourceRange = {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1},
    };
    cmd.pipelineBarrier(vk::PipelineStageFlagBits::eFragmentShader,
                        vk::PipelineStageFlagBits::eColorAttachmentOutput,
                        {}, {}, {}, toAttachment);

    vk::ClearValue clear;
    clear.color = vk::ClearColorValue(mClearColor_);
    vk::RenderPassBeginInfo pass{};
    pass.renderPass = mRenderPass_;
    pass.framebuffer = mFramebuffer_;
    pass.renderArea.offset = vk::Offset2D{0, 0};
    pass.renderArea.extent = vk::Extent2D{mWidth_, mHeight_};
    pass.clearValueCount = 1;
    pass.pClearValues = &clear;
    cmd.beginRenderPass(pass, vk::SubpassContents::eInline);

    // Full-target viewport/scissor as a default (GUI renderers usually set their own).
    cmd.setViewport(0, vk::Viewport{0.0f, 0.0f, static_cast<float>(mWidth_), static_cast<float>(mHeight_), 0.0f, 1.0f});
    cmd.setScissor(0, vk::Rect2D{{0, 0}, {mWidth_, mHeight_}});
}

void PanelRenderTargetXR::end(vk::CommandBuffer cmd) {
    cmd.endRenderPass();
    recordMipChain(cmd);
}

void PanelRenderTargetXR::recordMipChain(vk::CommandBuffer cmd) {
    auto barrier = [&](uint32_t baseMip, uint32_t mipCount,
                       vk::ImageLayout oldLayout, vk::ImageLayout newLayout,
                       vk::AccessFlags srcAccess, vk::AccessFlags dstAccess,
                       vk::PipelineStageFlags srcStage, vk::PipelineStageFlags dstStage) {
        vk::ImageMemoryBarrier b{
            .srcAccessMask = srcAccess,
            .dstAccessMask = dstAccess,
            .oldLayout = oldLayout,
            .newLayout = newLayout,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = mImage_,
            .subresourceRange = {vk::ImageAspectFlagBits::eColor, baseMip, mipCount, 0, 1},
        };
        cmd.pipelineBarrier(srcStage, dstStage, {}, {}, {}, b);
    };

    if (mMipLevels_ <= 1) {
        barrier(0, 1, vk::ImageLayout::eColorAttachmentOptimal, vk::ImageLayout::eShaderReadOnlyOptimal,
                vk::AccessFlagBits::eColorAttachmentWrite, vk::AccessFlagBits::eShaderRead,
                vk::PipelineStageFlagBits::eColorAttachmentOutput, vk::PipelineStageFlagBits::eFragmentShader);
        return;
    }

    // mip 0: rendered -> blit source; the rest: discarded -> blit destinations
    barrier(0, 1, vk::ImageLayout::eColorAttachmentOptimal, vk::ImageLayout::eTransferSrcOptimal,
            vk::AccessFlagBits::eColorAttachmentWrite, vk::AccessFlagBits::eTransferRead,
            vk::PipelineStageFlagBits::eColorAttachmentOutput, vk::PipelineStageFlagBits::eTransfer);
    barrier(1, mMipLevels_ - 1, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal,
            {}, vk::AccessFlagBits::eTransferWrite,
            vk::PipelineStageFlagBits::eFragmentShader, vk::PipelineStageFlagBits::eTransfer);

    int32_t w = static_cast<int32_t>(mWidth_);
    int32_t h = static_cast<int32_t>(mHeight_);
    for (uint32_t level = 1; level < mMipLevels_; ++level) {
        const int32_t nw = std::max(w / 2, 1);
        const int32_t nh = std::max(h / 2, 1);
        vk::ImageBlit blit{
            .srcSubresource = {vk::ImageAspectFlagBits::eColor, level - 1, 0, 1},
            .srcOffsets = std::array<vk::Offset3D, 2>{vk::Offset3D{0, 0, 0}, vk::Offset3D{w, h, 1}},
            .dstSubresource = {vk::ImageAspectFlagBits::eColor, level, 0, 1},
            .dstOffsets = std::array<vk::Offset3D, 2>{vk::Offset3D{0, 0, 0}, vk::Offset3D{nw, nh, 1}},
        };
        cmd.blitImage(mImage_, vk::ImageLayout::eTransferSrcOptimal,
                      mImage_, vk::ImageLayout::eTransferDstOptimal,
                      blit, mLinearBlit_ ? vk::Filter::eLinear : vk::Filter::eNearest);
        barrier(level, 1, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eTransferSrcOptimal,
                vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eTransferRead,
                vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eTransfer);
        w = nw;
        h = nh;
    }

    barrier(0, mMipLevels_, vk::ImageLayout::eTransferSrcOptimal, vk::ImageLayout::eShaderReadOnlyOptimal,
            vk::AccessFlagBits::eTransferRead | vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead,
            vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eFragmentShader);
}

} // namespace clay

#endif // CLAY_PLATFORM_XR
