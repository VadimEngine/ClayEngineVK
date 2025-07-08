#pragma once
// standard lib
#include <vector>
#include <array>
// clay
#include "clay/graphics/common/BaseGraphicsContext.h"
#include "clay/graphics/common/ShaderModule.h"

namespace clay {

class PipelineResource {
public:

    struct DescriptorSetLayoutInfo {
        std::vector<VkDescriptorSetLayoutBinding> bindings;
    };

    struct PipelineLayoutInfo {
        std::vector<ShaderModule*> shaders;
        VkVertexInputBindingDescription vertexInputBindingDescription;
        std::vector<VkVertexInputAttributeDescription> attributeDescriptions;
        VkPipelineDepthStencilStateCreateInfo depthStencilState;
        VkPipelineRasterizationStateCreateInfo rasterizerState;
        std::vector<VkPushConstantRange> pushConstants;
    };

    struct PipelineConfig {
        BaseGraphicsContext& graphicsContext;
        PipelineLayoutInfo pipelineLayoutInfo;
        DescriptorSetLayoutInfo bindingLayoutInfo;
    };

    PipelineResource(const PipelineConfig& config);

    // move constructor
    PipelineResource(PipelineResource&& other);

    // move assignment
    PipelineResource& operator=(PipelineResource&& other) noexcept;

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