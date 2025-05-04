// class
#include "clay/graphics/common/Texture.h"

namespace clay {

Texture::Texture(IGraphicsContext& gContext)
    : mGraphicsContext_(gContext) {}

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
}

void Texture::setSampler(VkSampler sampler) {
    mSampler_ = sampler;
}

} // namespace