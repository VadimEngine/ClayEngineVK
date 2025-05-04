#pragma once
// standard lib
#include <vector>
#include <array>
// clay
#include "clay/graphics/common/IGraphicsContext.h"

namespace clay {

class PipelineResource {
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

//    struct ImageCreateInfo {
//        void* data;
//        VkShaderStageFlags stageFlags;
//        VkDescriptorType descriptorType;
//        uint32_t binding;
//    };

    struct PipelineConfig {
        IGraphicsContext& graphicsContext;

        std::vector<VkPipelineShaderStageCreateInfo> shaderStages;

        VkVertexInputBindingDescription vertexInputBindingDescription;

        std::vector<VkVertexInputAttributeDescription> attributeDescriptions;

        std::vector<VkPushConstantRange> pushConstants;

        std::vector<BufferCreateInfo> bufferCreateInfos;

        std::vector<VkDescriptorSetLayoutBinding> imageCreateInfos;

        VkPipelineDepthStencilStateCreateInfo depthStencilState;

        VkPipelineRasterizationStateCreateInfo rasterizerState;
    };

    IGraphicsContext& mGraphicsContext_;
    VkPipelineLayout mPipelineLayout_;
    VkPipeline mPipeline_;
    VkDescriptorSetLayout mDescriptorSetLayout_;

    PipelineResource(const PipelineConfig& config);

    ~PipelineResource();

    void createDescriptorSetLayout(const PipelineConfig& config);

    void createPipeline(const PipelineConfig& config);
};

} // namespace clay