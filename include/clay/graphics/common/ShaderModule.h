#pragma once
// standard lib
#include <stdexcept>
// third party
// vulkan
#include <vulkan/vulkan.h>
// clay
#include "clay/utils/common/Utils.h"

namespace clay {

class ShaderModule {
public:
    ShaderModule(VkDevice device,  VkShaderStageFlagBits stage, const utils::FileData& fileData);

    ~ShaderModule();

    VkShaderModule getShaderModule() const;
    
    VkShaderStageFlagBits getStage() const;

private:
    VkDevice mDevice_;
    VkShaderStageFlagBits mStage_;
    VkShaderModule mShaderModule_{VK_NULL_HANDLE};
};

}// namespace clay
