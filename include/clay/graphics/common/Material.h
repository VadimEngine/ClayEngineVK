#pragma once
// standard lib
#include <vector>
#include <array>
// clay
#include "clay/application/common/Handle.h"
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

    // Return this material's descriptor set to the pool. Nothing does this automatically
    // (the destructor leaves it to the pool's own destruction), so an app that makes
    // materials per scene / per entity must call it when they are done with, or the pool
    // (GraphicsContext: 1024 sets) runs out. The device must be idle; the material must
    // not be drawn with again.
    void freeDescriptorSet();

    const PipelineResource& getPipelineResource() const { return mPipelineResource_; }

    // GPU instancing: the same material built on a pipeline that reads its model
    // matrix + colour per instance (see InstanceData.h). When set, the render system
    // draws every object sharing a mesh + this material in one instanced call.
    void setInstancedVariant(clay::Handle<Material> instanced) {
        mInstancedVariant_ = instanced;
        mHasInstancedVariant_ = true;
    }
    bool hasInstancedVariant() const { return mHasInstancedVariant_; }
    clay::Handle<Material> getInstancedVariant() const { return mInstancedVariant_; }

private:
    void createDescriptorSet(const MaterialConfig& config);

    BaseGraphicsContext& mGraphicsContext_;
    PipelineResource& mPipelineResource_; // TODO i think this should be a handle?
    vk::DescriptorSet mDescriptorSet_;
    clay::Handle<Material> mInstancedVariant_{};
    bool mHasInstancedVariant_ = false;

    std::vector<UniformBuffer> mUniformBuffers_;
};

} // namespace clay