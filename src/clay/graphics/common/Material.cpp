// standard lib
#include <stdexcept>
// class
#include "clay/graphics/common/Material.h"

namespace clay {

Material::Material(const MaterialConfig& config)
    : mGraphicsContext_(config.graphicsContext),
      mPipelineResource_(config.pipelineResource),
      mDescriptorSet_(VK_NULL_HANDLE) {
    createDescriptorSet(config);
}

void Material::bindMaterial(VkCommandBuffer cmdBuffer) const {
    vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, getPipeline());

    vkCmdBindDescriptorSets(
        cmdBuffer,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        getPipelineLayout(),
        0,
        1,
        &mDescriptorSet_,
        0,
        nullptr
    );
}

void Material::pushConstants(VkCommandBuffer cmdBuffer, const void* data, uint32_t size, VkShaderStageFlags stageFlags) const {
    vkCmdPushConstants(cmdBuffer, mPipelineResource_.getPipelineLayout(), stageFlags, 0, size, data);
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
    // VkDescriptorSet does not need to be finalized manually. It is freed when the VkDescriptorPool is finalized
}

void Material::createDescriptorSet(const MaterialConfig& config) {
    // Allocate descriptor set
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = mGraphicsContext_.mDescriptorPool_;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &mPipelineResource_.getDescriptorSetLayout();

    if (vkAllocateDescriptorSets(mGraphicsContext_.getDevice(), &allocInfo, &mDescriptorSet_) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate descriptor sets!");
    }

    std::vector<VkWriteDescriptorSet> descriptorWrites;
    std::vector<VkDescriptorBufferInfo> bufferInfos;
    std::vector<VkDescriptorImageInfo> imageInfos;

    // Handle buffer bindings
    for (const auto& binding : config.bufferBindings) {
        bufferInfos.push_back({
            .buffer = binding.buffer,
            .offset = 0,
            .range = binding.size
        });

        descriptorWrites.push_back({
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = mDescriptorSet_,
            .dstBinding = binding.binding,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = binding.descriptorType,
            .pBufferInfo = &bufferInfos.back()
        });
    }

    // Handle single image bindings
    for (const auto& binding : config.imageBindings) {
        imageInfos.push_back({
            .sampler = binding.sampler,
            .imageView = binding.imageView,
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        });

        descriptorWrites.push_back({
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
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
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        });
    }

    if (!config.imageArrayBindings.empty()) {
        descriptorWrites.push_back({
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = mDescriptorSet_,
            .dstBinding = config.imageArrayBindings.front().binding,
            .dstArrayElement = 0,
            .descriptorCount = static_cast<uint32_t>(config.imageArrayBindings.size()),
            .descriptorType = config.imageArrayBindings.front().descriptorType,
            .pImageInfo = &imageInfos[config.imageBindings.size()]
        });
    }

    vkUpdateDescriptorSets(
        mGraphicsContext_.getDevice(),
        static_cast<uint32_t>(descriptorWrites.size()),
        descriptorWrites.data(),
        0,
        nullptr
    );
}

VkPipeline Material::getPipeline() const {
    return mPipelineResource_.getPipeline();
}

VkPipelineLayout Material::getPipelineLayout() const {
    return mPipelineResource_.getPipelineLayout();
}

VkDescriptorSetLayout Material::getDescriptorSetLayout() const {
    return mPipelineResource_.getDescriptorSetLayout();
}

const VkDescriptorSet& Material::getDescriptorSet() const {
    return mDescriptorSet_;
}

} // namespace clay