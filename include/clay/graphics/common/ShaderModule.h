#pragma once
// standard lib
#include <stdexcept>
// third party
// vulkan
#include <vulkan/vulkan.hpp>
// clay
#include "clay/utils/common/Utils.h"

namespace clay {

class ShaderModule {
public:
    ShaderModule(vk::Device device, vk::ShaderStageFlagBits stage, const utils::FileData& fileData);

    ~ShaderModule();

    vk::ShaderModule getShaderModule() const;
    
    vk::ShaderStageFlagBits getStage() const;

private:
    vk::Device mDevice_;
    vk::ShaderStageFlagBits mStage_;
    vk::ShaderModule mShaderModule_;
};

}// namespace clay
