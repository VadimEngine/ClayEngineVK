#pragma once
// standard lib
#include <cstring> // memcpy
// third party
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan.h>
#include "clay/utils/common/Utils.h"

namespace clay {

class BaseGraphicsContext {
public:

    virtual ~BaseGraphicsContext();

    void createBuffer(
        vk::DeviceSize size, 
        vk::BufferUsageFlags usage, 
        vk::MemoryPropertyFlags properties,
        vk::Buffer& buffer, 
        vk::DeviceMemory& bufferMemory
    );

    void copyBuffer(vk::Buffer srcBuffer, vk::Buffer dstBuffer, vk::DeviceSize size);

    void createImage(
        uint32_t width,
        uint32_t height,
        uint32_t mipLevels,
        vk::SampleCountFlagBits numSamples,
        vk::Format format,
        vk::ImageTiling tiling,
        vk::ImageUsageFlags usage,
        vk::MemoryPropertyFlags properties,
        vk::Image& image,
        vk::DeviceMemory& imageMemory
    );

    void populateImage(vk::Image image, utils::ImageData& imageData);

    vk::ImageView createImageView(vk::Image image, vk::Format format, vk::ImageAspectFlags aspectFlags, uint32_t mipLevels);

    void transitionImageLayout(vk::Image image, vk::Format format, vk::ImageLayout oldLayout, vk::ImageLayout newLayout, uint32_t mipLevels);

    void copyBufferToImage(vk::Buffer buffer, vk::Image image, uint32_t width, uint32_t height);

    uint32_t findMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties);

    vk::CommandBuffer beginSingleTimeCommands();

    void endSingleTimeCommands(vk::CommandBuffer commandBuffer);

    vk::Device getDevice() const;

    vk::Instance getInstance() const;

    std::pair<int, int> getFrameDimensions() const;

protected:
    // initializer list instead
    vk::Device mDevice_ = nullptr;
    vk::Instance mInstance_ = nullptr;

    std::pair<int, int> mFrameDimensions_;
public:
    vk::PhysicalDevice mPhysicalDevice_ = nullptr;
    vk::RenderPass mRenderPass_ = nullptr;
    vk::DescriptorPool mDescriptorPool_ = nullptr;
    vk::CommandPool mCommandPool_ = nullptr;
    vk::Queue mGraphicsQueue_ = nullptr;
};

} // namespace clay