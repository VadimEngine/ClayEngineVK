#pragma once
// third party
#include <vulkan/vulkan.h>
#include "clay/utils/common/Utils.h"

namespace clay {

class IGraphicsContext {
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

    virtual ~IGraphicsContext() {};

    virtual void createBuffer(
        VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties,
        VkBuffer& buffer, VkDeviceMemory& bufferMemory
    ) = 0;

    virtual void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size) = 0;

    virtual void createImage(
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
    ) = 0;

    virtual VkSampler createSampler() = 0;

    virtual void populateImage(VkImage image, utils::ImageData& imageData) = 0;

    virtual VkImageView createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags, uint32_t mipLevels) = 0;

    virtual VkShaderModule createShader(const ShaderCreateInfo& shaderCI) = 0;



    virtual void transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout, uint32_t mipLevels) = 0;

    virtual void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height) = 0;

    virtual uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) = 0;


    VkInstance mInstance_{};

    VkPhysicalDevice mPhysicalDevice_ = VK_NULL_HANDLE;

    VkDevice mDevice_ = VK_NULL_HANDLE;

    VkRenderPass mRenderPass_ = VK_NULL_HANDLE;

    VkDescriptorPool mDescriptorPool_;

    VkCommandPool mCommandPool_{};

    VkQueue mQueue_{};

};

} // namespace clay