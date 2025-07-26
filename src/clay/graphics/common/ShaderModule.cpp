#include "clay/graphics/common/ShaderModule.h"

namespace clay {

ShaderModule::ShaderModule(vk::Device device, vk::ShaderStageFlagBits stage, const utils::FileData& fileData)
    : mDevice_(device),
      mStage_(stage) {

    mShaderModule_ = mDevice_.createShaderModule({
        .codeSize = fileData.size,
        .pCode = reinterpret_cast<const uint32_t*>(fileData.data.get())
    });
}

ShaderModule::~ShaderModule() {
    mDevice_.destroyShaderModule(mShaderModule_);
}

vk::ShaderModule ShaderModule::getShaderModule() const {
    return mShaderModule_;
}

vk::ShaderStageFlagBits ShaderModule::getStage() const {
    return mStage_;
}

}// namespace clay
