// class
#include "clay/graphics/common/Texture.h"

namespace clay {

Texture::Texture(BaseGraphicsContext& gContext)
    : mGraphicsContext_(gContext) {}

Texture::~Texture() {
    finalize();
}

void Texture::initialize(utils::ImageData& imageData) {
    mGraphicsContext_.createImage(
        imageData.width,
        imageData.height,
        1,
        VK_SAMPLE_COUNT_1_BIT,
        VK_FORMAT_R8G8B8A8_SRGB,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        mImage_,
        mImageMemory_
    );

    mGraphicsContext_.populateImage(mImage_, imageData);

    mImageView_ = mGraphicsContext_.createImageView(
        mImage_, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT, 1
    );
    // todo transition layout from VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL  to VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    // maybe give Texture an API to transition?
    mGraphicsContext_.transitionImageLayout(
        mImage_,
        VK_FORMAT_R8G8B8A8_SRGB,              // single channel 8-bit unsigned normalized
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        1
    );
}

void Texture::setSampler(VkSampler sampler) {
    mSampler_ = sampler;
}

void Texture::finalize() {
    if (mImageView_ != VK_NULL_HANDLE) {
        vkDestroyImageView(mGraphicsContext_.getDevice(), mImageView_, nullptr);
        mImageView_ = VK_NULL_HANDLE;
    }

    if (mImage_ != VK_NULL_HANDLE) {
        vkDestroyImage(mGraphicsContext_.getDevice(), mImage_, nullptr);
        mImage_ = VK_NULL_HANDLE;
    }

    if (mImageMemory_ != VK_NULL_HANDLE) {
        vkFreeMemory(mGraphicsContext_.getDevice(), mImageMemory_, nullptr);
        mImageMemory_ = VK_NULL_HANDLE;
    }
}

VkImageView Texture::getImageView() const {
    return mImageView_;
}

VkSampler Texture::getSampler() const {
    return mSampler_;
}

} // namespace