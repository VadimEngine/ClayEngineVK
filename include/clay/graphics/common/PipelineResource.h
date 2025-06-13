#pragma once
// standard lib
#include <vector>
#include <array>
// clay
#include "clay/graphics/common/BaseGraphicsContext.h"

namespace clay {

class PipelineResource {
public:

    struct DescriptorSetLayoutInfo {
        std::vector<VkDescriptorSetLayoutBinding> bindings;
    };

    struct PipelineLayoutInfo {
        std::vector<VkPipelineShaderStageCreateInfo> shaderStages;
        VkVertexInputBindingDescription vertexInputBindingDescription;
        std::vector<VkVertexInputAttributeDescription> attributeDescriptions;
        VkPipelineDepthStencilStateCreateInfo depthStencilState;
        VkPipelineRasterizationStateCreateInfo rasterizerState;
        std::vector<VkPushConstantRange> pushConstants;
    };

    struct PipelineConfig { // TODO group into descriptor set and pipeline for clarity
        BaseGraphicsContext& graphicsContext;
        PipelineLayoutInfo pipelineLayoutInfo;
        DescriptorSetLayoutInfo bindingLayoutInfo;
    };

    PipelineResource(const PipelineConfig& config);

    ~PipelineResource();

    const VkPipelineLayout& getPipelineLayout() const;

    const VkPipeline& getPipeline() const;

    const VkDescriptorSetLayout& getDescriptorSetLayout() const;

private:
    void createDescriptorSetLayout(const PipelineConfig& config);

    void createPipeline(const PipelineConfig& config);

    void finalize();

    BaseGraphicsContext& mGraphicsContext_;
    VkPipelineLayout mPipelineLayout_;
    VkPipeline mPipeline_;
    VkDescriptorSetLayout mDescriptorSetLayout_;
};

} // namespace clay