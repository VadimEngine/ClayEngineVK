// class
#include "clay/graphics/common/Texture.h"

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

void Texture::initialize(utils::ImageData& imageData) {
    mGraphicsContext_.createImage(
        imageData.width,
        imageData.height,
        1,
        vk::SampleCountFlagBits::e1,
        vk::Format::eR8G8B8A8Srgb,
        vk::ImageTiling::eOptimal,
        vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled,
        vk::MemoryPropertyFlagBits::eDeviceLocal,
        mImage_,
        mImageMemory_
    );

    mGraphicsContext_.populateImage(mImage_, imageData);

    mImageView_ = mGraphicsContext_.createImageView(
        mImage_, vk::Format::eR8G8B8A8Srgb, vk::ImageAspectFlagBits::eColor, 1
    );

    // maybe give Texture an API to transition?
    mGraphicsContext_.transitionImageLayout(
        mImage_,
        vk::Format::eR8G8B8A8Srgb,
        vk::ImageLayout::eTransferDstOptimal,
        vk::ImageLayout::eShaderReadOnlyOptimal,
        1
    );
}

void Texture::setSampler(vk::Sampler sampler) {
    mSampler_ = sampler;
}

void Texture::finalize() {
    if (mImageView_ != nullptr) {
        mGraphicsContext_.getDevice().destroyImageView(mImageView_);
        mImageView_ = nullptr;
    }

    if (mImage_ != nullptr) {
        mGraphicsContext_.getDevice().destroyImage(mImage_);
        mImage_ = nullptr;
    }

    if (mImageMemory_ != nullptr) {
        mGraphicsContext_.getDevice().freeMemory(mImageMemory_);
        mImageMemory_ = nullptr;
    }
}

vk::ImageView Texture::getImageView() const {
    return mImageView_;
}

vk::Sampler Texture::getSampler() const {
    return mSampler_;
}

} // namespace