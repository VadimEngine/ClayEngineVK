// third party
#include <stdexcept>
// class
#include "clay/graphics/common/UniformBuffer.h"

namespace clay {

UniformBuffer::UniformBuffer(BaseGraphicsContext& graphicsContext, vk::DeviceSize size, void* data) 
    : mGraphicsContext_(graphicsContext){
    mSize_ = size;
    vk::BufferCreateInfo bufferInfo{
       .size = size,
       .usage = vk::BufferUsageFlagBits::eUniformBuffer,
       .sharingMode = vk::SharingMode::eExclusive
    };

    mBuffer_ = mGraphicsContext_.getDevice().createBuffer(bufferInfo);

    vk::MemoryRequirements memRequirements = mGraphicsContext_.getDevice().getBufferMemoryRequirements(mBuffer_);

    vk::MemoryAllocateInfo allocInfo{
        .allocationSize = memRequirements.size,
        .memoryTypeIndex = mGraphicsContext_.findMemoryType(
            memRequirements.memoryTypeBits, 
            vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent // TODO see if this should be dynamic
        )
    };

    mBufferMemory_ = mGraphicsContext_.getDevice().allocateMemory(allocInfo);

   mGraphicsContext_.getDevice().bindBufferMemory(mBuffer_, mBufferMemory_, 0);

    mBufferMapped_ = mGraphicsContext_.getDevice().mapMemory(
        mBufferMemory_,
        0,
        size
    );

    if (data != nullptr) {
        memcpy(mBufferMapped_, data, size);
    }
}

// Move constructor
UniformBuffer::UniformBuffer(UniformBuffer&& other) noexcept
    : mGraphicsContext_(other.mGraphicsContext_),
        mBuffer_(other.mBuffer_),
        mBufferMemory_(other.mBufferMemory_),
        mBufferMapped_(other.mBufferMapped_) {
    other.mBuffer_ = nullptr;
    other.mBufferMemory_ = nullptr;
    other.mBufferMapped_ = nullptr;
}

// Move assignment
UniformBuffer& UniformBuffer::operator=(UniformBuffer&& other) noexcept {
    if (this != &other) {
        finalize();

        mGraphicsContext_ = other.mGraphicsContext_;
        mBuffer_ = other.mBuffer_;
        mBufferMemory_ = other.mBufferMemory_;
        mBufferMapped_ = other.mBufferMapped_;

        other.mBuffer_ = nullptr;
        other.mBufferMemory_ = nullptr;
        other.mBufferMapped_ = nullptr;
    }
    return *this;
}

UniformBuffer::~UniformBuffer() {
    finalize();
}

void UniformBuffer::setData(void* data, size_t size) {
    std::memcpy(mBufferMapped_, data, size);
}

vk::DeviceSize UniformBuffer::getSize() const {
    return mSize_;
}

void UniformBuffer::finalize() {
    if (mBufferMapped_) {
        mGraphicsContext_.getDevice().unmapMemory(mBufferMemory_);
        mBufferMapped_ = nullptr;
    }
    if (mBuffer_ != nullptr) {
        mGraphicsContext_.getDevice().destroyBuffer(mBuffer_);
        mBuffer_ = nullptr;
    }
    if (mBufferMemory_ != nullptr) {
        mGraphicsContext_.getDevice().freeMemory(mBufferMemory_);
        mBufferMemory_ = nullptr;
    }
}


} // namespace clay