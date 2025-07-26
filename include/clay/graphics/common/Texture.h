#pragma once
// clay
#include "clay/graphics/common/BaseGraphicsContext.h"
#include "clay/utils/common/Utils.h"

namespace clay {

class Texture {
public:
    Texture(BaseGraphicsContext& gContext);

    // move constructor
    Texture(Texture&& other) noexcept;

    // move assignment
    Texture& operator=(Texture&& other) noexcept;

    ~Texture();

    void initialize(utils::ImageData& imageData);

    void setSampler(vk::Sampler sampler);

    vk::ImageView getImageView() const;

    vk::Sampler getSampler() const;
    
    void finalize();

private:
    BaseGraphicsContext& mGraphicsContext_;

    vk::Image mImage_;
    vk::DeviceMemory mImageMemory_;
    vk::ImageView mImageView_;
    vk::Sampler mSampler_; // does not own

};


} // namespace clay