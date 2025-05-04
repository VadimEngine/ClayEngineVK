#pragma once
// clay
#include "clay/graphics/common/IGraphicsContext.h"
#include "clay/utils/common/Utils.h"

namespace clay {

class Texture {
public:
    Texture(IGraphicsContext& gContext);

    void initialize(utils::ImageData& imageData);

    void setSampler(VkSampler sampler);

    IGraphicsContext& mGraphicsContext_;

    VkImage mImage_;
    VkDeviceMemory mImageMemory_;
    VkImageView mImageView_;
    VkSampler mSampler_;

};


} // namespace clay