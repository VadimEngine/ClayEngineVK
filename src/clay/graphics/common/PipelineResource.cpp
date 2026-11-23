// standard lib
#include <stdexcept>
// class
#include "clay/graphics/common/PipelineResource.h"

namespace clay {

PipelineResource::PipelineResource(const PipelineConfig& config)
    : mGraphicsContext_(config.graphicsContext),
      mPipelineLayout_(nullptr),
      mPipeline_(nullptr),
      mDescriptorSetLayout_(nullptr),
      mDepthTestedAndWritten_(config.pipelineLayoutInfo.depthStencilState.depthTestEnable &&
                              config.pipelineLayoutInfo.depthStencilState.depthWriteEnable) {
    createDescriptorSetLayout(config);
    createPipeline(config);
}

// move constructor
PipelineResource::PipelineResource(PipelineResource&& other)
    : mGraphicsContext_(other.mGraphicsContext_){
    mPipelineLayout_ = other.mPipelineLayout_;
    mPipeline_ = other.mPipeline_;
    mDescriptorSetLayout_ = other.mDescriptorSetLayout_;
    mDepthTestedAndWritten_ = other.mDepthTestedAndWritten_;

    other.mPipelineLayout_ = nullptr;
    other.mPipeline_ = nullptr;
    other.mDescriptorSetLayout_ = nullptr;
}

// move assignment
PipelineResource& PipelineResource::operator=(PipelineResource&& other) noexcept {
    if (this != &other) {
        finalize();
        mPipelineLayout_ = other.mPipelineLayout_;
        mPipeline_ = other.mPipeline_;
        mDescriptorSetLayout_ = other.mDescriptorSetLayout_;
        mDepthTestedAndWritten_ = other.mDepthTestedAndWritten_;

        other.mPipelineLayout_ = nullptr;
        other.mPipeline_ = nullptr;
        other.mDescriptorSetLayout_ = nullptr;
    }
    return *this;
}

PipelineResource::~PipelineResource() {
    finalize();
}

const vk::PipelineLayout& PipelineResource::getPipelineLayout() const {
    return mPipelineLayout_;
}

const vk::Pipeline& PipelineResource::getPipeline() const {
    return mPipeline_;
}

const vk::DescriptorSetLayout& PipelineResource::getDescriptorSetLayout() const {
    return mDescriptorSetLayout_;
}

void PipelineResource::createDescriptorSetLayout(const PipelineConfig& config) {
    vk::DescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.bindingCount = static_cast<uint32_t>(config.bindingLayoutInfo.bindings.size());
    layoutInfo.pBindings = config.bindingLayoutInfo.bindings.data();

    mDescriptorSetLayout_ = mGraphicsContext_.getDevice().createDescriptorSetLayout(layoutInfo);
}

void PipelineResource::createPipeline(const PipelineConfig& config) {
    std::vector<vk::VertexInputBindingDescription> vertexBindings{
        config.pipelineLayoutInfo.vertexInputBindingDescription
    };
    vertexBindings.insert(vertexBindings.end(),
                          config.pipelineLayoutInfo.extraVertexBindings.begin(),
                          config.pipelineLayoutInfo.extraVertexBindings.end());

    vk::PipelineVertexInputStateCreateInfo vertexInputInfo{
        .vertexBindingDescriptionCount = static_cast<uint32_t>(vertexBindings.size()),
        .pVertexBindingDescriptions = vertexBindings.data(),
        .vertexAttributeDescriptionCount = static_cast<uint32_t>(config.pipelineLayoutInfo.attributeDescriptions.size()),
        .pVertexAttributeDescriptions = config.pipelineLayoutInfo.attributeDescriptions.data(),
    };

    vk::PipelineInputAssemblyStateCreateInfo inputAssembly{
        .topology = vk::PrimitiveTopology::eTriangleList,
        .primitiveRestartEnable = false
    };

    vk::PipelineViewportStateCreateInfo viewportState{
        .viewportCount = 1,
        .scissorCount = 1
    };

    vk::PipelineMultisampleStateCreateInfo multisampling{
        .rasterizationSamples = vk::SampleCountFlagBits::e1,
        .sampleShadingEnable = false,
    };

    vk::PipelineColorBlendAttachmentState colorBlendAttachment{
        .blendEnable = true,
        // Colour blend factors are configurable (default: standard src-alpha over)
        // so e.g. an additive-bloom pipeline can request eOne / eOne.
        .srcColorBlendFactor = config.pipelineLayoutInfo.srcColorBlendFactor,
        .dstColorBlendFactor = config.pipelineLayoutInfo.dstColorBlendFactor,
        .colorBlendOp = vk::BlendOp::eAdd,
        .srcAlphaBlendFactor = vk::BlendFactor::eOne,
        .dstAlphaBlendFactor = vk::BlendFactor::eZero,
        .alphaBlendOp = vk::BlendOp::eAdd,
        .colorWriteMask = config.pipelineLayoutInfo.colorWriteMask,
    };

    vk::PipelineColorBlendStateCreateInfo colorBlending{
        .logicOpEnable = false,
        .logicOp = vk::LogicOp::eCopy,
        .attachmentCount = config.pipelineLayoutInfo.colorAttachmentCount,
        .pAttachments = config.pipelineLayoutInfo.colorAttachmentCount > 0 ? &colorBlendAttachment : nullptr,
        .blendConstants = vk::ArrayWrapper1D<float, 4>({ 0.0f, 0.0f, 0.0f, 0.0f })
    };

    std::vector<vk::DynamicState> dynamicStates = {
        vk::DynamicState::eViewport,
        vk::DynamicState::eScissor
    };

    vk::PipelineDynamicStateCreateInfo dynamicState{
        .dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()),
        .pDynamicStates = dynamicStates.data()
    };

    vk::PipelineLayoutCreateInfo pipelineLayoutInfo{
        .setLayoutCount = 1,
        .pSetLayouts = &mDescriptorSetLayout_,
        .pushConstantRangeCount =  static_cast<uint32_t>(config.pipelineLayoutInfo.pushConstants.size()),
        .pPushConstantRanges = config.pipelineLayoutInfo.pushConstants.data()
    };

    mPipelineLayout_ = mGraphicsContext_.getDevice().createPipelineLayout(pipelineLayoutInfo);

    std::vector<vk::PipelineShaderStageCreateInfo> shaderStages;

    for (auto& eachShader: config.pipelineLayoutInfo.shaders) {
        shaderStages.push_back({
            .stage = eachShader->getStage(),
            .module = eachShader->getShaderModule(),
            .pName = "main"
        });
    }

    vk::GraphicsPipelineCreateInfo pipelineInfo{
        .stageCount =  static_cast<uint32_t>(shaderStages.size()),
        .pStages = shaderStages.data(),
        .pVertexInputState = &vertexInputInfo,
        .pInputAssemblyState = &inputAssembly,
        .pViewportState = &viewportState,
        .pRasterizationState = &config.pipelineLayoutInfo.rasterizerState,
        .pMultisampleState = &multisampling,
        .pDepthStencilState = &config.pipelineLayoutInfo.depthStencilState,
        .pColorBlendState = &colorBlending,
        .pDynamicState = &dynamicState,
        .layout = mPipelineLayout_,
        .renderPass = config.pipelineLayoutInfo.renderPassOverride
                          ? config.pipelineLayoutInfo.renderPassOverride
                          : mGraphicsContext_.mRenderPass_,
        .subpass = 0,
        .basePipelineHandle = nullptr
    };

    mPipeline_ = mGraphicsContext_.getDevice().createGraphicsPipeline(nullptr, pipelineInfo).value;
}

void PipelineResource::finalize() {
    if (mPipeline_) {
        mGraphicsContext_.getDevice().destroyPipeline(mPipeline_, nullptr);
        mPipeline_ = nullptr;
    }
    if (mPipelineLayout_) {
        mGraphicsContext_.getDevice().destroyPipelineLayout(mPipelineLayout_, nullptr);
        mPipelineLayout_ = nullptr;
    }
    if (mDescriptorSetLayout_) {
        mGraphicsContext_.getDevice().destroyDescriptorSetLayout(mDescriptorSetLayout_, nullptr);
        mDescriptorSetLayout_ = nullptr;
    }
}

} // namespace clay