// standard lib
#include <stdexcept>
// class
#include "clay/graphics/common/BaseGraphicsContext.h"

namespace clay {

BaseGraphicsContext::~BaseGraphicsContext() {}

void BaseGraphicsContext::populateImage(vk::Image image, utils::ImageData& imageData) {
    vk::Buffer stagingBuffer;
    vk::DeviceMemory stagingBufferMemory;
    createBuffer(
        imageData.width * imageData.height * imageData.channels,
        vk::BufferUsageFlagBits::eTransferSrc,
        vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
        stagingBuffer,
        stagingBufferMemory
    );

    vk::DeviceSize imageSize = imageData.width * imageData.height * imageData.channels;

    void* data = mDevice_.mapMemory(stagingBufferMemory, 0, imageSize);
    memcpy(data, imageData.pixels.get(), static_cast<size_t>(imageSize));
    mDevice_.unmapMemory(stagingBufferMemory);

    transitionImageLayout(
        image,
        vk::Format::eR8G8B8A8Sint,// TODO this should be a parameter
        vk::ImageLayout::eUndefined,
        vk::ImageLayout::eTransferDstOptimal,
        1
    );

    copyBufferToImage(
        stagingBuffer,
        image,
        static_cast<uint32_t>(imageData.width),
        static_cast<uint32_t>(imageData.height)
    );

    mDevice_.destroyBuffer(stagingBuffer);
    mDevice_.freeMemory(stagingBufferMemory);
}

vk::ImageView BaseGraphicsContext::createImageView(vk::Image image, vk::Format format, vk::ImageAspectFlags aspectFlags, uint32_t mipLevels) {
    vk::ImageViewCreateInfo viewInfo{
        .image = image,
        .viewType =  vk::ImageViewType::e2D,
        .format = format,
        .subresourceRange = {
            .aspectMask = aspectFlags,
            .baseMipLevel = 0,
            .levelCount = mipLevels,
            .baseArrayLayer = 0,
            .layerCount = 1
        }
    };

    return mDevice_.createImageView(viewInfo);
}

vk::CommandBuffer BaseGraphicsContext::beginSingleTimeCommands() {
    vk::CommandBufferAllocateInfo allocInfo{
        .commandPool = mCommandPool_,
        .level = vk::CommandBufferLevel::ePrimary,
        .commandBufferCount = 1,
    };

    vk::CommandBuffer commandBuffer = mDevice_.allocateCommandBuffers(allocInfo).front();

    vk::CommandBufferBeginInfo beginInfo{
        .sType = vk::StructureType::eCommandBufferBeginInfo,
        .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit
    };

    commandBuffer.begin(beginInfo);

    return commandBuffer;
}

void BaseGraphicsContext::endSingleTimeCommands(vk::CommandBuffer commandBuffer) {
    commandBuffer.end();

    vk::SubmitInfo submitInfo{
        .commandBufferCount = 1,
        .pCommandBuffers = &commandBuffer
    };

    mGraphicsQueue_.submit(submitInfo);
    mGraphicsQueue_.waitIdle();
    mDevice_.freeCommandBuffers(mCommandPool_, 1, &commandBuffer);
}

void BaseGraphicsContext::transitionImageLayout(
    vk::Image image,
    vk::Format format,
    vk::ImageLayout oldLayout,
    vk::ImageLayout newLayout,
    uint32_t mipLevels
) {
    vk::CommandBuffer commandBuffer = beginSingleTimeCommands();

    vk::ImageMemoryBarrier barrier{
        .oldLayout = oldLayout,
        .newLayout = newLayout,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = image,
        .subresourceRange = {
            .aspectMask =  vk::ImageAspectFlagBits::eColor,
            .baseMipLevel = 0,
            .levelCount = mipLevels,
            .baseArrayLayer = 0,
            .layerCount = 1
        }
    };

    vk::PipelineStageFlags sourceStage;
    vk::PipelineStageFlags destinationStage;
    
    if (oldLayout == vk::ImageLayout::eUndefined && newLayout == vk::ImageLayout::eTransferDstOptimal) {
        barrier.srcAccessMask = vk::AccessFlagBits::eNone; // 0 or vk::AccessFlags{}
        barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;

        sourceStage = vk::PipelineStageFlagBits::eTopOfPipe;
        destinationStage = vk::PipelineStageFlagBits::eTransfer;
    } else if (oldLayout == vk::ImageLayout::eTransferDstOptimal && newLayout == vk::ImageLayout::eShaderReadOnlyOptimal) {
        barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
        barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

        sourceStage = vk::PipelineStageFlagBits::eTransfer;
        destinationStage = vk::PipelineStageFlagBits::eFragmentShader;
    } else if (oldLayout == vk::ImageLayout::eUndefined && newLayout == vk::ImageLayout::eShaderReadOnlyOptimal) {
        barrier.srcAccessMask = vk::AccessFlagBits::eNone ;//0;
        barrier.dstAccessMask = vk::AccessFlagBits:: eShaderRead;

        sourceStage = vk::PipelineStageFlagBits::eTopOfPipe;
        destinationStage = vk::PipelineStageFlagBits::eFragmentShader;
    } else if (oldLayout == vk::ImageLayout::eUndefined && newLayout == vk::ImageLayout::eColorAttachmentOptimal) {
        barrier.srcAccessMask = vk::AccessFlagBits::eNone; //0;
        barrier.dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;

        sourceStage = vk::PipelineStageFlagBits::eTopOfPipe;
        destinationStage = vk::PipelineStageFlagBits::eColorAttachmentOutput;
    } else if (oldLayout == vk::ImageLayout::eColorAttachmentOptimal && newLayout == vk::ImageLayout::eShaderReadOnlyOptimal) {
        barrier.srcAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;
        barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

        sourceStage = vk::PipelineStageFlagBits::eColorAttachmentOutput;
        destinationStage = vk::PipelineStageFlagBits::eFragmentShader;
    } else if (oldLayout == vk::ImageLayout::eShaderReadOnlyOptimal && newLayout == vk::ImageLayout::eColorAttachmentOptimal) {
        barrier.srcAccessMask = vk::AccessFlagBits::eShaderRead;
        barrier.dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;

        sourceStage = vk::PipelineStageFlagBits::eFragmentShader;
        destinationStage = vk::PipelineStageFlagBits::eColorAttachmentOutput;
    } else {
        throw std::invalid_argument("unsupported layout transition!");
    }

    commandBuffer.pipelineBarrier(
        sourceStage,
        destinationStage,
        {},
        nullptr,
        nullptr,
        { barrier }
    );

    endSingleTimeCommands(commandBuffer);
}

void BaseGraphicsContext::copyBufferToImage(vk::Buffer buffer, vk::Image image, uint32_t width, uint32_t height) {
    vk::CommandBuffer commandBuffer = beginSingleTimeCommands();

    vk::BufferImageCopy region{
        .bufferOffset = 0,
        .bufferRowLength = 0,
        .bufferImageHeight = 0,
        .imageSubresource = {
            .aspectMask = vk::ImageAspectFlagBits::eColor,
            .mipLevel = 0,
            .baseArrayLayer = 0,
            .layerCount = 1,
        },
        .imageOffset = {0, 0, 0},
        .imageExtent = {
            width,
            height,
            1
        }
    };

    commandBuffer.copyBufferToImage(buffer, image, vk::ImageLayout::eTransferDstOptimal, 1, &region);

    endSingleTimeCommands(commandBuffer);
}

uint32_t BaseGraphicsContext::findMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties) {
    vk::PhysicalDeviceMemoryProperties memProperties;

    memProperties = mPhysicalDevice_.getMemoryProperties();

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }

    throw std::runtime_error("failed to find suitable memory type!");
}

void BaseGraphicsContext::createImage(uint32_t width,
                                      uint32_t height,
                                      uint32_t mipLevels, 
                                      vk::SampleCountFlagBits numSamples, 
                                      vk::Format format, 
                                      vk::ImageTiling tiling, 
                                      vk::ImageUsageFlags usage, 
                                      vk::MemoryPropertyFlags properties, 
                                      vk::Image& image, 
                                      vk::DeviceMemory& imageMemory) {
    vk::ImageCreateInfo imageInfo{
        .imageType = vk::ImageType::e2D,
        .format = format,
        .extent = {
            .width = width,
            .height = height,
            .depth = 1,
        },
        .mipLevels = mipLevels,
        .arrayLayers = 1,
        .samples = numSamples,
        .tiling = tiling,
        .usage = usage,
        .sharingMode = vk::SharingMode::eExclusive,
        .initialLayout = vk::ImageLayout::eUndefined,
    };

    image = mDevice_.createImage(imageInfo);

    vk::MemoryRequirements memRequirements;
    memRequirements = mDevice_.getImageMemoryRequirements(image);

    vk::MemoryAllocateInfo allocInfo{
        .allocationSize = memRequirements.size,
        .memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties)
    };

    imageMemory = mDevice_.allocateMemory(allocInfo);

    mDevice_.bindImageMemory(image, imageMemory, 0);
}

void BaseGraphicsContext::copyBuffer(vk::Buffer srcBuffer, vk::Buffer dstBuffer, vk::DeviceSize size) {
    vk::CommandBuffer commandBuffer = beginSingleTimeCommands();

    vk::BufferCopy copyRegion{
        .size = size
    };
    commandBuffer.copyBuffer(srcBuffer, dstBuffer, 1, &copyRegion);

    endSingleTimeCommands(commandBuffer);
}

void BaseGraphicsContext::createBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags properties, vk::Buffer& buffer, vk::DeviceMemory& bufferMemory) {
    vk::BufferCreateInfo bufferInfo{
        .size = size,
        .usage = usage,
        .sharingMode = vk::SharingMode::eExclusive
    };

    buffer = mDevice_.createBuffer(bufferInfo);

    vk::MemoryRequirements memRequirements = mDevice_.getBufferMemoryRequirements(buffer);

    vk::MemoryAllocateInfo allocInfo{
        .allocationSize = memRequirements.size,
        .memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties)
    };

    bufferMemory = mDevice_.allocateMemory(allocInfo);

    mDevice_.bindBufferMemory(buffer, bufferMemory, 0);
}

vk::Device BaseGraphicsContext::getDevice() const {
    return mDevice_;
}

vk::Instance BaseGraphicsContext::getInstance() const {
    return mInstance_;
}

std::pair<int, int> BaseGraphicsContext::getFrameDimensions() const {
    return mFrameDimensions_;
}

} // namespace clay