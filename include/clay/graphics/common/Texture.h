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

    void setSampler(VkSampler sampler);

    VkImageView getImageView() const;

    VkSampler getSampler() const;
    
    void finalize();

private:
    BaseGraphicsContext& mGraphicsContext_;

    VkImage mImage_;
    VkDeviceMemory mImageMemory_;
    VkImageView mImageView_;
    VkSampler mSampler_; // does not own

};


} // namespace clay