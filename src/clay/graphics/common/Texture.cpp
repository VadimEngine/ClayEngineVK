// class
#include "clay/graphics/common/Texture.h"
// standard lib
#include <algorithm>
#include <array>
#include <cmath>

namespace clay {

Texture::Texture(BaseGraphicsContext& gContext)
    : mGraphicsContext_(gContext) {}

Texture::Texture(Texture&& other) noexcept
    : mGraphicsContext_(other.mGraphicsContext_) {
    mImage_ = other.mImage_;
    mImageMemory_ = other.mImageMemory_;
    mImageView_ = other.mImageView_;
    mSampler_ = other.mSampler_;

    other.mImage_ = nullptr;
    other.mImageMemory_ = nullptr;
    other.mImageView_ = nullptr;
    other.mSampler_ = nullptr;
}

Texture& Texture::operator=(Texture&& other) noexcept {
    if (this != &other) {
        mImage_ = other.mImage_;
        mImageMemory_ = other.mImageMemory_;
        mImageView_ = other.mImageView_;
        mSampler_ = other.mSampler_;

        other.mImage_ = nullptr;
        other.mImageMemory_ = nullptr;
        other.mImageView_ = nullptr;
        other.mSampler_ = nullptr;
    }
    return *this;
}

Texture::~Texture() {
    finalize();
}

void Texture::initialize(utils::ImageData& imageData, bool generateMipmaps) {
    const vk::Format format = vk::Format::eR8G8B8A8Srgb;
    uint32_t mipLevels = 1;
    bool linearBlit = true;
    if (generateMipmaps) {
        const auto features = mGraphicsContext_.mPhysicalDevice_.getFormatProperties(format).optimalTilingFeatures;
        if ((features & vk::FormatFeatureFlagBits::eBlitSrc) && (features & vk::FormatFeatureFlagBits::eBlitDst)) {
            mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(imageData.width, imageData.height)))) + 1;
        }
        linearBlit = static_cast<bool>(features & vk::FormatFeatureFlagBits::eSampledImageFilterLinear);
    }

    mGraphicsContext_.createImage(
        imageData.width,
        imageData.height,
        mipLevels,
        vk::SampleCountFlagBits::e1,
        vk::Format::eR8G8B8A8Srgb,
        vk::ImageTiling::eOptimal,
        vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled,
        vk::MemoryPropertyFlagBits::eDeviceLocal,
        mImage_,
        mImageMemory_
    );

    mGraphicsContext_.populateImage(mImage_, imageData); // mip 0, left in eTransferDstOptimal

    mImageView_ = mGraphicsContext_.createImageView(
        mImage_, format, vk::ImageAspectFlagBits::eColor, mipLevels
    );

    if (mipLevels <= 1) {
        // maybe give Texture an API to transition?
        mGraphicsContext_.transitionImageLayout(
            mImage_,
            format,
            vk::ImageLayout::eTransferDstOptimal,
            vk::ImageLayout::eShaderReadOnlyOptimal,
            1
        );
        return;
    }

    // Mip chain: each level blitted (box-filtered) from the one above, then all shader-readable.
    vk::CommandBuffer cmd = mGraphicsContext_.beginSingleTimeCommands();
    auto barrier = [&](uint32_t baseMip, uint32_t count, vk::ImageLayout from, vk::ImageLayout to,
                       vk::AccessFlags srcAccess, vk::AccessFlags dstAccess,
                       vk::PipelineStageFlags srcStage, vk::PipelineStageFlags dstStage) {
        vk::ImageMemoryBarrier b{
            .srcAccessMask = srcAccess,
            .dstAccessMask = dstAccess,
            .oldLayout = from,
            .newLayout = to,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = mImage_,
            .subresourceRange = {vk::ImageAspectFlagBits::eColor, baseMip, count, 0, 1},
        };
        cmd.pipelineBarrier(srcStage, dstStage, {}, {}, {}, b);
    };
    barrier(0, 1, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eTransferSrcOptimal,
            vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eTransferRead,
            vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eTransfer);
    barrier(1, mipLevels - 1, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal,
            {}, vk::AccessFlagBits::eTransferWrite,
            vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eTransfer);
    int32_t w = imageData.width;
    int32_t h = imageData.height;
    for (uint32_t level = 1; level < mipLevels; ++level) {
        const int32_t nw = std::max(w / 2, 1);
        const int32_t nh = std::max(h / 2, 1);
        vk::ImageBlit blit{
            .srcSubresource = {vk::ImageAspectFlagBits::eColor, level - 1, 0, 1},
            .srcOffsets = std::array<vk::Offset3D, 2>{vk::Offset3D{0, 0, 0}, vk::Offset3D{w, h, 1}},
            .dstSubresource = {vk::ImageAspectFlagBits::eColor, level, 0, 1},
            .dstOffsets = std::array<vk::Offset3D, 2>{vk::Offset3D{0, 0, 0}, vk::Offset3D{nw, nh, 1}},
        };
        cmd.blitImage(mImage_, vk::ImageLayout::eTransferSrcOptimal, mImage_, vk::ImageLayout::eTransferDstOptimal,
                      blit, linearBlit ? vk::Filter::eLinear : vk::Filter::eNearest);
        barrier(level, 1, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eTransferSrcOptimal,
                vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eTransferRead,
                vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eTransfer);
        w = nw;
        h = nh;
    }
    barrier(0, mipLevels, vk::ImageLayout::eTransferSrcOptimal, vk::ImageLayout::eShaderReadOnlyOptimal,
            vk::AccessFlagBits::eTransferRead, vk::AccessFlagBits::eShaderRead,
            vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eFragmentShader);
    mGraphicsContext_.endSingleTimeCommands(cmd);
}

void Texture::setSampler(vk::Sampler sampler) {
    mSampler_ = sampler;
}

void Texture::finalize() {
    if (mImageView_) {
        mGraphicsContext_.getDevice().destroyImageView(mImageView_, nullptr);
        mImageView_ = vk::ImageView{};
    }

    if (mImage_) {
        mGraphicsContext_.getDevice().destroyImage(mImage_, nullptr);
        mImage_ = vk::Image{};
    }

    if (mImageMemory_) {
        mGraphicsContext_.getDevice().freeMemory(mImageMemory_, nullptr);
        mImageMemory_ = vk::DeviceMemory{};
    }
}

vk::ImageView Texture::getImageView() const {
    return mImageView_;
}

vk::Sampler Texture::getSampler() const {
    return mSampler_;
}

} // namespace