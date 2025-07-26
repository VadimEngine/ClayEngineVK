// standard lib
#include <stdexcept>
// class
#include "clay/graphics/common/Material.h"

namespace clay {

Material::Material(const MaterialConfig& config)
    : mGraphicsContext_(config.graphicsContext),
      mPipelineResource_(config.pipelineResource),
      mDescriptorSet_(nullptr) {
    createDescriptorSet(config);
}

void Material::bindMaterial(vk::CommandBuffer cmdBuffer) const {
    cmdBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, getPipeline());

    cmdBuffer.bindDescriptorSets(
        vk::PipelineBindPoint::eGraphics,
        getPipelineLayout(),
        0,
        1,
        &mDescriptorSet_,
        0,
        nullptr
    );
}

void Material::pushConstants(vk::CommandBuffer cmdBuffer, const void* data, uint32_t size, vk::ShaderStageFlags stageFlags) const {
    cmdBuffer.pushConstants(mPipelineResource_.getPipelineLayout(), stageFlags, 0, size, data);
}

// move constructor
Material::Material(Material&& other) 
    : mGraphicsContext_(other.mGraphicsContext_),
      mPipelineResource_(other.mPipelineResource_),
      mDescriptorSet_(other.mDescriptorSet_) {}

// move assignment
Material& Material::operator=(Material&& other) noexcept {
    if (this != &other) {
        mDescriptorSet_ = other.mDescriptorSet_;
    }
    return *this;
}

Material::~Material() {
    // vk::DescriptorSet does not need to be finalized manually. It is freed when the vk::DescriptorPool is finalized
}

void Material::createDescriptorSet(const MaterialConfig& config) {
    // Allocate descriptor set
    vk::DescriptorSetAllocateInfo allocInfo{};
    allocInfo.descriptorPool = mGraphicsContext_.mDescriptorPool_;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &((vk::DescriptorSetLayout&)mPipelineResource_.getDescriptorSetLayout());
    
    if (mGraphicsContext_.getDevice().allocateDescriptorSets(&allocInfo, &mDescriptorSet_) != vk::Result::eSuccess) {
        throw std::runtime_error("failed to allocate descriptor sets!");
    }

    std::vector<vk::WriteDescriptorSet> descriptorWrites;
    std::vector<vk::DescriptorBufferInfo> bufferInfos;
    std::vector<vk::DescriptorImageInfo> imageInfos;

    // Handle buffer bindings
    for (const auto& binding : config.bufferBindings) {
        bufferInfos.push_back({
            .buffer = binding.buffer,
            .offset = 0,
            .range = binding.size
        });

        descriptorWrites.push_back({
            .dstSet = mDescriptorSet_,
            .dstBinding = binding.binding,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = binding.descriptorType,
            .pBufferInfo = &bufferInfos.back(),
        });
    }

    // Handle single image bindings
    for (const auto& binding : config.imageBindings) {
        imageInfos.push_back({
            .sampler = binding.sampler,
            .imageView = binding.imageView,
            .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal
        });

        descriptorWrites.push_back({
            .dstSet = mDescriptorSet_,
            .dstBinding = binding.binding,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = binding.descriptorType,
            .pImageInfo = &imageInfos.back()
        });
    }

    // Handle image arrays
    for (const auto& binding : config.imageArrayBindings) {
        imageInfos.push_back({
            .sampler = binding.sampler,
            .imageView = binding.imageView,
            .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal
        });
    }

    if (!config.imageArrayBindings.empty()) {
        descriptorWrites.push_back({
            .dstSet = mDescriptorSet_,
            .dstBinding = config.imageArrayBindings.front().binding,
            .dstArrayElement = 0,
            .descriptorCount = static_cast<uint32_t>(config.imageArrayBindings.size()),
            .descriptorType = config.imageArrayBindings.front().descriptorType,
            .pImageInfo = &imageInfos[config.imageBindings.size()]
        });
    }

    mGraphicsContext_.getDevice().updateDescriptorSets(
        static_cast<uint32_t>(descriptorWrites.size()),
        descriptorWrites.data(),
        0,
        nullptr
    );
}

vk::Pipeline Material::getPipeline() const {
    return mPipelineResource_.getPipeline();
}

vk::PipelineLayout Material::getPipelineLayout() const {
    return mPipelineResource_.getPipelineLayout();
}

vk::DescriptorSetLayout Material::getDescriptorSetLayout() const {
    return mPipelineResource_.getDescriptorSetLayout();
}

const vk::DescriptorSet& Material::getDescriptorSet() const {
    return mDescriptorSet_;
}

} // namespace clay