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
        // Further vertex buffer bindings beyond the mesh's own (binding 0) — e.g. the
        // per-instance stream InstanceData::getBindingDescription() for instanced draws.
        std::vector<vk::VertexInputBindingDescription> extraVertexBindings;
        std::vector<vk::VertexInputAttributeDescription> attributeDescriptions;
        vk::PipelineDepthStencilStateCreateInfo depthStencilState{};
        vk::PipelineRasterizationStateCreateInfo rasterizerState{};
        std::vector<vk::PushConstantRange> pushConstants;
        // Which colour channels the pipeline writes. Default RGBA; set to {} for a
        // stencil/depth-only pass that must not touch the colour buffer.
        vk::ColorComponentFlags colorWriteMask =
            vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
            vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
        // Colour blend factors (colorBlendOp is always eAdd). Defaults give the
        // usual src-alpha "over" blend; set both to eOne for additive bloom.
        vk::BlendFactor srcColorBlendFactor = vk::BlendFactor::eSrcAlpha;
        vk::BlendFactor dstColorBlendFactor = vk::BlendFactor::eOneMinusSrcAlpha;
        // Render pass this pipeline targets. Null = the main swapchain pass; set to
        // an offscreen pass (e.g. the shadow-map pass) to render into that instead.
        vk::RenderPass renderPassOverride = nullptr;
        // Number of colour attachments the target render pass has. Set to 0 for a
        // depth-only pass (e.g. the shadow map) so the colour-blend state matches.
        uint32_t colorAttachmentCount = 1;
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

    // True when the pipeline both depth-tests and depth-writes. Opaque draws on such a
    // pipeline are order-independent, so the renderer may reorder them to cut state changes.
    bool isDepthTestedAndWritten() const { return mDepthTestedAndWritten_; }

private:
    void createDescriptorSetLayout(const PipelineConfig& config);

    void createPipeline(const PipelineConfig& config);

    void finalize();

    BaseGraphicsContext& mGraphicsContext_;
    vk::PipelineLayout mPipelineLayout_;
    vk::Pipeline mPipeline_;
    vk::DescriptorSetLayout mDescriptorSetLayout_;
    bool mDepthTestedAndWritten_ = false;
};

} // namespace clay