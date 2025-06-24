#include "clay/graphics/common/ShaderModule.h"

namespace clay {

ShaderModule::ShaderModule(VkDevice device, VkShaderStageFlagBits stage, const utils::FileData& fileData)
    : mDevice_(device),
      mStage_(stage) {
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = fileData.size;
    createInfo.pCode = reinterpret_cast<const uint32_t*>(fileData.data.get());

    if (vkCreateShaderModule(mDevice_, &createInfo, nullptr, &mShaderModule_) != VK_SUCCESS) {
        throw std::runtime_error("failed to create shader module!");
    }
}

ShaderModule::~ShaderModule() {
    vkDestroyShaderModule(mDevice_, mShaderModule_, nullptr);
}

VkShaderModule ShaderModule::getShaderModule() const {
    return mShaderModule_;
}

    
VkShaderStageFlagBits ShaderModule::getStage() const {
    return mStage_;
}

}// namespace clay
