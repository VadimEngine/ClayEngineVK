#pragma once

// standard lib
#include <iostream>
#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <cstdlib>
#include <optional>
#include <vector>
#include <fstream>
#include <set>
#include <array>
#include <chrono>
// third party
// move all glfw to window.h?
#define GLFW_INCLUDE_VULKAN 
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>
// glm
//#define GLM_FORCE_RADIANS
//#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
// clay
#include "clay/gui/Window.h"
#include "clay/utils/Utils.h"
#include "clay/application/Scene.h"
#include "clay/graphics/Material.h"

struct QueueFamilyIndices {
    std::optional<uint32_t> graphicsFamily;
    std::optional<uint32_t> presentFamily;

    inline bool isComplete() {
        return graphicsFamily.has_value();
    }
};

struct SwapChainSupportDetails {
    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;
};

struct UniformBufferObject {
    alignas(16) glm::mat4 model;
    alignas(16) glm::mat4 view;
    alignas(16) glm::mat4 proj;
};

class App {
public:
    static std::vector<char> readFile(const std::string& filename);

    static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
        VkDebugUtilsMessageTypeFlagsEXT messageType,
        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
        void* pUserData);


    App();

    ~App();

    void run();

    Window& getWindow();

//private:
    void initVulkan();

    void mainLoop();

    void cleanupSwapChain();
    
    void cleanup();

    void recreateSwapChain();

    void createInstance();

    void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo);

    void setupDebugMessenger();

    void createSurface();

    void pickPhysicalDevice();

    void createLogicalDevice();

    void createSwapChain();

    void createImageViews();

    void createRenderPass();

    void createDescriptorSetLayout();

    void createGraphicsPipeline();

    void createFramebuffers();

    void createCommandPool();

    void createColorResources();

    void createDepthResources();

    VkFormat findSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features);

    VkFormat findDepthFormat();

    bool hasStencilComponent(VkFormat format);

    void createTextureImage();

    VkSampleCountFlagBits getMaxUsableSampleCount();

    void generateMipmaps(VkImage image, VkFormat imageFormat, int32_t texWidth, int32_t texHeight, uint32_t mipLevels);

    void createTextureImageView();

    void createTextureSampler();

    VkImageView createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags, uint32_t mipLevels);

    void createImage(uint32_t width, uint32_t height, uint32_t mipLevels, VkSampleCountFlagBits numSamples, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory);

    void transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout, uint32_t mipLevels);

    VkCommandBuffer beginSingleTimeCommands();

    void endSingleTimeCommands(VkCommandBuffer commandBuffer);

    void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);

    void loadModel();

    void createVertexBuffer();

    void createIndexBuffer();

    void createUniformBuffers();

    void createDescriptorPool();

    void createDescriptorSets();

    void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory);

    void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);

    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);

    void createCommandBuffers();

    void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex);

    void createSyncObjects();

    void updateUniformBuffer(uint32_t currentImage);

    void drawFrame();

    VkShaderModule createShaderModule(const std::vector<char>& code);

    VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);

    VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);

    VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);

    SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device);

    bool isDeviceSuitable(VkPhysicalDevice device);

    bool checkDeviceExtensionSupport(VkPhysicalDevice device);

    QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device);

    std::vector<const char*> getRequiredExtensions();

    bool checkValidationLayerSupport();

    Scene mScene_;

    Window mWindow_;
    VkInstance mInstance_;
    VkDebugUtilsMessengerEXT mDebugMessenger_;
    VkSurfaceKHR mSurface_;
    VkPhysicalDevice mPhysicalDevice_ = VK_NULL_HANDLE;
    VkSampleCountFlagBits mMSAASamples_ = VK_SAMPLE_COUNT_1_BIT;
    VkDevice mDevice_; // logical device
    
    VkQueue mGraphicsQueue_;
    VkQueue mPresentQueue_;

    VkSwapchainKHR mSwapChain_;
    std::vector<VkImage> mSwapChainImages_;
    VkFormat mSwapChainImageFormat_;
    VkExtent2D mSwapChainExtent_;

    std::vector<VkImageView> mSwapChainImageViews_;
    std::vector<VkFramebuffer> mSwapChainFramebuffers_;

    Material mMaterial_;
    VkRenderPass mRenderPass_;
    VkDescriptorSetLayout mDescriptorSetLayout_;
    VkPipelineLayout mPipelineLayout_;
    //VkPipeline mGraphicsPipeline_;

    VkCommandPool mCommandPool_;

    VkImage mColorImage_;
    VkDeviceMemory mColorImageMemory_;
    VkImageView mColorImageView_;

    VkImage mDepthImage_;
    VkDeviceMemory mDepthImageMemory_;
    VkImageView mDepthImageView_;

    uint32_t mMipLevels_;
    VkImage mTextureImage_;
    VkDeviceMemory mTextureImageMemory_;
    VkImageView mTextureImageView_;
    VkSampler mTextureSampler_;

    std::vector<Vertex> mVertices_;
    std::vector<uint32_t> mIndices_;
    VkBuffer vertexBuffer;
    VkDeviceMemory mVertexBufferMemory_;
    VkBuffer mIndexBuffer_;
    VkDeviceMemory mIndexBufferMemory_;

    std::vector<VkBuffer> mUniformBuffers_;
    std::vector<VkDeviceMemory> mUniformBuffersMemory_;
    std::vector<void*> mUniformBuffersMapped_;

    VkDescriptorPool mDescriptorPool_;
    //std::vector<VkDescriptorSet> mDescriptorSets_;

    std::vector<VkCommandBuffer> mCommandBuffers_;

    std::vector<VkSemaphore> mImageAvailableSemaphores_;
    std::vector<VkSemaphore> mRenderFinishedSemaphores_;
    std::vector<VkFence> mInFlightFences_;
    uint32_t mCurrentFrame_ = 0;
    bool mFramebufferResized_ = false;

    std::chrono::steady_clock::time_point mLastTime_;

};