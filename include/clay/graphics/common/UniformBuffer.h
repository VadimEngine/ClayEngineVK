#pragma once
#include "clay/graphics/common/BaseGraphicsContext.h"

namespace clay {

class UniformBuffer {
public:
    UniformBuffer(BaseGraphicsContext& graphicsContext, vk::DeviceSize size, void* data);

    UniformBuffer(const UniformBuffer&) = delete;
    UniformBuffer& operator=(const UniformBuffer&) = delete;
    
    ~UniformBuffer();

    // Move constructor
    UniformBuffer(UniformBuffer&& other) noexcept;

    // Move assignment
    UniformBuffer& operator=(UniformBuffer&& other) noexcept;

    void setData(void* data, size_t size);

    void finalize();

    vk::DeviceSize getSize() const;

    BaseGraphicsContext& mGraphicsContext_;
    vk::DeviceSize mSize_;
    vk::Buffer mBuffer_;
    vk::DeviceMemory mBufferMemory_;
    void* mBufferMapped_;
};

} // namespace clay