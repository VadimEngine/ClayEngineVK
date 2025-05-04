// standard lib
#include <stdexcept>
// class
#include "clay/graphics/common/PipelineResource.h"

namespace clay {

PipelineResource::PipelineResource(const PipelineConfig& config)
    : mGraphicsContext_(config.graphicsContext),
      mPipelineLayout_(VK_NULL_HANDLE),
      mPipeline_(VK_NULL_HANDLE),
      mDescriptorSetLayout_(VK_NULL_HANDLE) {
    createDescriptorSetLayout(config);
    createPipeline(config);
}

PipelineResource::~PipelineResource() {

}

void PipelineResource::createDescriptorSetLayout(const PipelineConfig& config) {
    // create uniform buffers
    std::vector<VkDescriptorSetLayoutBinding> bindings;

    for (const auto& bufferCreateInfo : config.bufferCreateInfos) {
        VkDescriptorSetLayoutBinding uboLayoutBinding{};
        uboLayoutBinding.binding = bufferCreateInfo.binding;
        uboLayoutBinding.descriptorCount = 1; // for arrays
        uboLayoutBinding.descriptorType = bufferCreateInfo.descriptorType;
        uboLayoutBinding.pImmutableSamplers = nullptr;
        uboLayoutBinding.stageFlags = bufferCreateInfo.stageFlags;

        bindings.push_back(uboLayoutBinding);
    }

    for (const auto& imageCreateInfo : config.imageCreateInfos) {
//        VkDescriptorSetLayoutBinding uboLayoutBinding{};
//        uboLayoutBinding.binding = imageCreateInfo.binding;
//        uboLayoutBinding.descriptorCount = 1; // for arrays
//        uboLayoutBinding.descriptorType = imageCreateInfo.descriptorType;
//        uboLayoutBinding.pImmutableSamplers = nullptr;
//        uboLayoutBinding.stageFlags = imageCreateInfo.stageFlags;

        bindings.push_back(imageCreateInfo);
    }

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = bindings.size();
    layoutInfo.pBindings = bindings.data();

    if (vkCreateDescriptorSetLayout(
        mGraphicsContext_.mDevice_, &layoutInfo, nullptr, &mDescriptorSetLayout_
    ) != VK_SUCCESS) {
        throw std::runtime_error("failed to create descriptor set layout!");
    }
}

void PipelineResource::createPipeline(const PipelineConfig& config) {
    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(config.attributeDescriptions.size());
    vertexInputInfo.pVertexAttributeDescriptions = config.attributeDescriptions.data();
    vertexInputInfo.pVertexBindingDescriptions = &config.vertexInputBindingDescription;

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_TRUE;
    colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
    colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.logicOp = VK_LOGIC_OP_COPY;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;
    colorBlending.blendConstants[0] = 0.0f;
    colorBlending.blendConstants[1] = 0.0f;
    colorBlending.blendConstants[2] = 0.0f;
    colorBlending.blendConstants[3] = 0.0f;

    std::vector<VkDynamicState> dynamicStates = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };
    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
    dynamicState.pDynamicStates = dynamicStates.data();

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &mDescriptorSetLayout_;
    pipelineLayoutInfo.pushConstantRangeCount = config.pushConstants.size();
    pipelineLayoutInfo.pPushConstantRanges = config.pushConstants.data();

    if (vkCreatePipelineLayout(mGraphicsContext_.mDevice_, &pipelineLayoutInfo, nullptr, &mPipelineLayout_) != VK_SUCCESS) {
        throw std::runtime_error("failed to create pipeline layout!");
    }

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = config.shaderStages.size();
    pipelineInfo.pStages = config.shaderStages.data();
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &config.rasterizerState;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pDepthStencilState = &config.depthStencilState;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.layout = mPipelineLayout_;
    pipelineInfo.renderPass = mGraphicsContext_.mRenderPass_;
    pipelineInfo.subpass = 0;
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

    VkResult result = vkCreateGraphicsPipelines(mGraphicsContext_.mDevice_, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &mPipeline_);

    if (result != VK_SUCCESS) {
        throw std::runtime_error("failed to create graphics pipeline!");
    }
}

} // namespace clay