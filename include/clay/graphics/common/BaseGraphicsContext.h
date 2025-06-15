#pragma once
// third party
#include <vulkan/vulkan.h>
#include "clay/utils/common/Utils.h"

namespace clay {

class BaseGraphicsContext {
public:

    struct BufferCreateInfo {
        VkBufferUsageFlagBits type;
        size_t stride;
        size_t size;
        void* data;
    };

    struct ShaderCreateInfo {
        VkShaderStageFlagBits type;
        const unsigned char *sourceData;
        size_t sourceSize;
    };

    virtual ~BaseGraphicsContext();

    void createBuffer(
        VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties,
        VkBuffer& buffer, VkDeviceMemory& bufferMemory
    );

    void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);

    void createImage(
        uint32_t width,
        uint32_t height,
        uint32_t mipLevels,
        VkSampleCountFlagBits numSamples,
        VkFormat format,
        VkImageTiling tiling,
        VkImageUsageFlags usage,
        VkMemoryPropertyFlags properties,
        VkImage& image,
        VkDeviceMemory& imageMemory
    );

    void populateImage(VkImage image, utils::ImageData& imageData);

    VkImageView createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags, uint32_t mipLevels);

    VkShaderModule createShader(const ShaderCreateInfo& shaderCI);

    void transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout, uint32_t mipLevels);

    void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);

    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);

    VkCommandBuffer beginSingleTimeCommands();

    void endSingleTimeCommands(VkCommandBuffer commandBuffer);

    VkDevice getDevice() const;

    VkInstance getInstance() const;

    std::pair<int, int> getFrameDimensions() const;

protected:
    VkDevice mDevice_ = VK_NULL_HANDLE;
    VkInstance mInstance_ = VK_NULL_HANDLE;

    std::pair<int, int> mFrameDimensions_;
public:
    VkPhysicalDevice mPhysicalDevice_ = VK_NULL_HANDLE;
    VkRenderPass mRenderPass_ = VK_NULL_HANDLE;
    VkDescriptorPool mDescriptorPool_ = VK_NULL_HANDLE;
    VkCommandPool mCommandPool_ = VK_NULL_HANDLE;
    VkQueue mGraphicsQueue_ = VK_NULL_HANDLE;
};

} // namespace clay