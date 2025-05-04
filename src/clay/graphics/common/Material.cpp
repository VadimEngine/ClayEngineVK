// standard lib
#include <stdexcept>
// class
#include "clay/graphics/common/Material.h"

namespace clay {

Material::Material(const MaterialConfig& config)
    : mGraphicsContext_(config.graphicsContext),
      mPipelineResource_(config.pipelineResource),
      mDescriptorSet_(VK_NULL_HANDLE)
{
    createDescriptorSet(config);
}

Material::~Material() {}

void Material::createDescriptorSet(const MaterialConfig& config) {
    // create uniform buffers TODO add logic for shared uniform buffers. Buffers made outside of here
    // and are shadred between different materials/DescriptorSet
    std::vector<VkDescriptorBufferInfo> bufferInfos;
    std::vector<VkDescriptorImageInfo> imageInfos;

    for (int i = 0; i < config.bufferCreateInfos.size(); ++i) {
        auto& bufferCreateInfo = config.bufferCreateInfos[i];
        mUniformBuffers_.emplace_back();
        mUniformBuffersMemory_.emplace_back();
        mUniformBuffersMapped_.emplace_back();

        mGraphicsContext_.createBuffer(
            bufferCreateInfo.size,
            bufferCreateInfo.usage,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            mUniformBuffers_.back(),
            mUniformBuffersMemory_.back()
        );

        vkMapMemory(
            mGraphicsContext_.mDevice_,
            mUniformBuffersMemory_.back(),
            0,
            bufferCreateInfo.size,
            0,
            &mUniformBuffersMapped_.back()
        );

        bufferInfos.push_back(
            {
                .buffer = mUniformBuffers_[i],
                .offset = 0,
                .range = bufferCreateInfo.size
            }
        );
    }

    for (const auto& imageCreateInfo : config.imageCreateInfos) {
        imageInfos.push_back({
            .sampler = imageCreateInfo.sampler,
            .imageView = imageCreateInfo.imageView,
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        });
    }

    for (const auto& imageCreateInfo : config.imageCreateInfosArray) {
        imageInfos.push_back({
             .sampler = imageCreateInfo.sampler,
             .imageView = imageCreateInfo.imageView,
             .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
         });
    }

    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = mGraphicsContext_.mDescriptorPool_;
    allocInfo.descriptorSetCount = static_cast<uint32_t>(1);
    allocInfo.pSetLayouts = &mPipelineResource_.mDescriptorSetLayout_;

    if (vkAllocateDescriptorSets(mGraphicsContext_.mDevice_, &allocInfo, &mDescriptorSet_) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate descriptor sets!");
    }

    std::vector<VkWriteDescriptorSet> descriptorWrites;

    for (int i = 0; i < bufferInfos.size(); ++i) {
        descriptorWrites.push_back({
           .sType= VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
           .dstSet = mDescriptorSet_,
           .dstBinding = config.bufferCreateInfos[i].binding,
           .dstArrayElement = 0,
           .descriptorCount = 1,
           .descriptorType = config.bufferCreateInfos[i].descriptorType,
           .pBufferInfo = &bufferInfos[i]
       });
    }



    if (!config.imageCreateInfosArray.empty()) {
        descriptorWrites.push_back({
           .sType= VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
           .dstSet = mDescriptorSet_,
           .dstBinding = config.imageCreateInfosArray[0].binding,
           .dstArrayElement = 0,
           .descriptorCount = static_cast<uint32_t>(imageInfos.size()),
           .descriptorType = config.imageCreateInfosArray[0].descriptorType,
           .pImageInfo = imageInfos.data()
       });
    } else {
        // TODO figure out how to do this with arrays vs several individual images with different bindings
        for (int i = 0; i < imageInfos.size(); ++i) {
            descriptorWrites.push_back({
               .sType= VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
               .dstSet = mDescriptorSet_,
               .dstBinding = config.imageCreateInfos[i].binding,
               .dstArrayElement = 0,
               .descriptorCount = 1,
               .descriptorType = config.imageCreateInfos[i].descriptorType,
               .pImageInfo = &imageInfos[i]
           });
        }
    }

    vkUpdateDescriptorSets(
        mGraphicsContext_.mDevice_,
        static_cast<uint32_t>(descriptorWrites.size()),
        descriptorWrites.data(),
        0,
        nullptr
    );
}

VkPipeline Material::getPipeline() const {
    return mPipelineResource_.mPipeline_;
}

VkPipelineLayout Material::getPipelineLayout() const {
    return mPipelineResource_.mPipelineLayout_;
}

VkDescriptorSetLayout Material::getDescriptorSetLayout() const {
    return mPipelineResource_.mDescriptorSetLayout_;
}


} // namespace clay