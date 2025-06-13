#pragma once
// standard lib
#include <vector>
#include <array>
// clay
#include "clay/graphics/common/BaseGraphicsContext.h"
#include "clay/graphics/common/PipelineResource.h"
#include "clay/graphics/common/UniformBuffer.h"

namespace clay {

class Material {
public:
    struct BufferBindingInfo {
        VkBuffer buffer;
        VkDeviceSize size;
        uint32_t binding;
        VkDescriptorType descriptorType;
    };

    struct ImageBindingInfo {
        VkSampler sampler;
        VkImageView imageView;
        uint32_t binding;
        VkDescriptorType descriptorType;
    };

    // TODO this could be used to tell the material to make its own uniforms
    struct BufferCreateInfo {
        VkBufferUsageFlags usage;
        size_t stride;
        size_t size;
        VkShaderStageFlags stageFlags;
        VkDescriptorType descriptorType;
        uint32_t binding;
    };

    struct MaterialConfig {
        BaseGraphicsContext& graphicsContext;
        PipelineResource& pipelineResource;

        std::vector<BufferBindingInfo> bufferBindings;
        std::vector<ImageBindingInfo> imageBindings;
        std::vector<ImageBindingInfo> imageArrayBindings;
    };

    Material(const MaterialConfig& config);

    ~Material();

    void bindMaterial(VkCommandBuffer cmdBuffer) const;

    void pushConstants(VkCommandBuffer cmdBuffer, const void* data, uint32_t size, VkShaderStageFlags stageFlags) const;

    VkPipeline getPipeline() const;

    VkPipelineLayout getPipelineLayout() const;

    VkDescriptorSetLayout getDescriptorSetLayout() const;

    const VkDescriptorSet& getDescriptorSet() const;
    
private:
    void createDescriptorSet(const MaterialConfig& config);

    BaseGraphicsContext& mGraphicsContext_;
    PipelineResource& mPipelineResource_;
    VkDescriptorSet mDescriptorSet_;

    std::vector<UniformBuffer> mUniformBuffers_;
};

} // namespace clay