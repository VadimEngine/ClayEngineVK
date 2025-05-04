#pragma once
// standard lib
#include <vector>
#include <array>
// clay
#include "clay/graphics/common/IGraphicsContext.h"
#include "clay/graphics/common/PipelineResource.h"

namespace clay {

class Material {
public:

    struct BufferCreateInfo {
        VkBufferUsageFlags usage;
        size_t stride;
        size_t size;
        void* data;
        VkShaderStageFlags stageFlags;
        VkDescriptorType descriptorType;
        uint32_t binding;
    };

    struct ImageCreateInfo {
        void* data;
        VkShaderStageFlags stageFlags;
        VkDescriptorType descriptorType;
        VkImageView imageView;
        VkSampler sampler;
        uint32_t binding;
    };

    struct MaterialConfig {
        IGraphicsContext& graphicsContext;

        PipelineResource& pipelineResource;

        std::vector<BufferCreateInfo> bufferCreateInfos;

        std::vector<ImageCreateInfo> imageCreateInfos;

        std::vector<ImageCreateInfo> imageCreateInfosArray;

    };

    Material(const MaterialConfig& config);

    ~Material();

    void createDescriptorSet(const MaterialConfig& config);

    VkPipeline getPipeline() const;

    VkPipelineLayout getPipelineLayout() const;

    VkDescriptorSetLayout getDescriptorSetLayout() const;

    IGraphicsContext& mGraphicsContext_;
    PipelineResource& mPipelineResource_;

    VkDescriptorSet mDescriptorSet_;

    std::vector<VkBuffer> mUniformBuffers_;
    std::vector<VkDeviceMemory> mUniformBuffersMemory_;
    std::vector<void*> mUniformBuffersMapped_;
};

} // namespace clay