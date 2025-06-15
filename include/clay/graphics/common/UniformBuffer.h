#pragma once
#include "clay/graphics/common/BaseGraphicsContext.h"

namespace clay {

class UniformBuffer {
public:
    UniformBuffer(BaseGraphicsContext& graphicsContext, VkDeviceSize size, void* data);

    UniformBuffer(const UniformBuffer&) = delete;
    UniformBuffer& operator=(const UniformBuffer&) = delete;
    
    ~UniformBuffer();

    // Move constructor
    UniformBuffer(UniformBuffer&& other) noexcept;

    // Move assignment
    UniformBuffer& operator=(UniformBuffer&& other) noexcept;


    void setData(void* data, size_t size);

    void finalize();

    VkDeviceSize getSize_() const;

    VkDeviceSize mSize_;
    BaseGraphicsContext& mGraphicsContext_;
    VkBuffer mBuffer_;
    VkDeviceMemory mBufferMemory_;
    void* mBufferMapped_;
};

} // namespace clay