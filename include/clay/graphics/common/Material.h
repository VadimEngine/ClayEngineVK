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
        vk::Buffer buffer;
        vk::DeviceSize size;
        uint32_t binding;
        vk::DescriptorType descriptorType;
    };

    struct ImageBindingInfo {
        vk::Sampler sampler;
        vk::ImageView imageView;
        uint32_t binding;
        vk::DescriptorType descriptorType;
    };

    // TODO this could be used to tell the material to make its own uniforms
    struct BufferCreateInfo {
        vk::BufferUsageFlags usage;
        size_t stride;
        size_t size;
        vk::ShaderStageFlags stageFlags;
        vk::DescriptorType descriptorType;
        uint32_t binding;
    };

    struct MaterialConfig {
        BaseGraphicsContext& graphicsContext;
        PipelineResource& pipelineResource; // TODO i think this should be a resource handle?

        std::vector<BufferBindingInfo> bufferBindings;
        std::vector<ImageBindingInfo> imageBindings;
        std::vector<ImageBindingInfo> imageArrayBindings;
    };

    Material(const MaterialConfig& config);

    // move constructor
    Material(Material&& other);

    // move assignment
    Material& operator=(Material&& other) noexcept;

    ~Material();

    // TODO rename to bind (same for mesh)
    void bindMaterial(vk::CommandBuffer cmdBuffer) const;

    void pushConstants(vk::CommandBuffer cmdBuffer, const void* data, uint32_t size, vk::ShaderStageFlags stageFlags) const;

    vk::Pipeline getPipeline() const;

    vk::PipelineLayout getPipelineLayout() const;

    vk::DescriptorSetLayout getDescriptorSetLayout() const;

    const vk::DescriptorSet& getDescriptorSet() const;
    
private:
    void createDescriptorSet(const MaterialConfig& config);

    BaseGraphicsContext& mGraphicsContext_;
    PipelineResource& mPipelineResource_; // TODO i think this should be a handle?
    vk::DescriptorSet mDescriptorSet_;

    std::vector<UniformBuffer> mUniformBuffers_;
};

} // namespace clay