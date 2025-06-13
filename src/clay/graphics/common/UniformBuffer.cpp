// third party
#include <stdexcept>
// class
#include "clay/graphics/common/UniformBuffer.h"

namespace clay {

UniformBuffer::UniformBuffer(BaseGraphicsContext& graphicsContext, VkDeviceSize size, void* data) 
    : mGraphicsContext_(graphicsContext){
    mSize_ = size;
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(graphicsContext.getDevice(), &bufferInfo, nullptr, &mBuffer_) != VK_SUCCESS) {
        throw std::runtime_error("failed to create buffer!");
    }

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(graphicsContext.getDevice(), mBuffer_, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = mGraphicsContext_.findMemoryType(
        memRequirements.memoryTypeBits, 
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT // TODO see if this should be dynamic
    );

    if (vkAllocateMemory(graphicsContext.getDevice(), &allocInfo, nullptr, &mBufferMemory_) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate buffer memory!");
    }

    vkBindBufferMemory(graphicsContext.getDevice(), mBuffer_, mBufferMemory_, 0);

    vkMapMemory(
        mGraphicsContext_.getDevice(),
        mBufferMemory_,
        0,
        size,
        0,
        &mBufferMapped_
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
    other.mBuffer_ = VK_NULL_HANDLE;
    other.mBufferMemory_ = VK_NULL_HANDLE;
    other.mBufferMapped_ = nullptr;
}

// Move assignment
UniformBuffer& UniformBuffer::operator=(UniformBuffer&& other) noexcept {
    if (this != &other) {
        finalize(); // Free existing Vulkan resources

        mGraphicsContext_ = other.mGraphicsContext_;
        mBuffer_ = other.mBuffer_;
        mBufferMemory_ = other.mBufferMemory_;
        mBufferMapped_ = other.mBufferMapped_;

        other.mBuffer_ = VK_NULL_HANDLE;
        other.mBufferMemory_ = VK_NULL_HANDLE;
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

VkDeviceSize UniformBuffer::getSize_() const {
    return mSize_;
}

void UniformBuffer::finalize() {
    if (mBufferMapped_) {
        vkUnmapMemory(mGraphicsContext_.getDevice(), mBufferMemory_);
        mBufferMapped_ = nullptr;
    }
    if (mBuffer_ != VK_NULL_HANDLE) {
        vkDestroyBuffer(mGraphicsContext_.getDevice(), mBuffer_, nullptr);
        mBuffer_ = VK_NULL_HANDLE;
    }
    if (mBufferMemory_ != VK_NULL_HANDLE) {
        vkFreeMemory(mGraphicsContext_.getDevice(), mBufferMemory_, nullptr);
        mBufferMemory_ = VK_NULL_HANDLE;
    }
}


} // namespace clay