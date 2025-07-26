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
        std::vector<vk::DescriptorSetLayoutBinding> bindings;
    };

    struct PipelineLayoutInfo {
        std::vector<ShaderModule*> shaders;
        vk::VertexInputBindingDescription vertexInputBindingDescription{};
        std::vector<vk::VertexInputAttributeDescription> attributeDescriptions;
        vk::PipelineDepthStencilStateCreateInfo depthStencilState{};
        vk::PipelineRasterizationStateCreateInfo rasterizerState{};
        std::vector<vk::PushConstantRange> pushConstants;
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

    const vk::PipelineLayout& getPipelineLayout() const;

    const vk::Pipeline& getPipeline() const;

    const vk::DescriptorSetLayout& getDescriptorSetLayout() const;

private:
    void createDescriptorSetLayout(const PipelineConfig& config);

    void createPipeline(const PipelineConfig& config);

    void finalize();

    BaseGraphicsContext& mGraphicsContext_;
    vk::PipelineLayout mPipelineLayout_;
    vk::Pipeline mPipeline_;
    vk::DescriptorSetLayout mDescriptorSetLayout_;
};

} // namespace clay