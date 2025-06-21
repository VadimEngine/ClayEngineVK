#pragma once

#ifdef CLAY_PLATFORM_ANDROID
// standard lib
#include <stdexcept>
#include <vector>
#include <array>
#include <optional>
// third party
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_android.h>
#include <android/log.h>
#include <android_native_app_glue.h>
// clay
#include "clay/graphics/common/UniformBuffer.h"
#include "clay/graphics/common/BaseGraphicsContext.h"

namespace clay {

class GraphicsContextAndroid : public BaseGraphicsContext {
public:

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

    static constexpr int MAX_FRAMES_IN_FLIGHT = 2;

    GraphicsContextAndroid(android_app* pAndroidApp);

    ~GraphicsContextAndroid();

    void initialize(android_app* pAndroidApp);

    void setupDebugMessenger();

    void setSurface(VkSurfaceKHR surface);

    SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device);

    bool checkDeviceExtensionSupport(VkPhysicalDevice device);

    bool isDeviceSuitable(VkPhysicalDevice device);

    void createLogicalDevice();

    QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device);

    void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo);

    std::vector<const char*> getRequiredExtensions();

    bool checkValidationLayerSupport();

    void createDescriptorPool();

    VkSampleCountFlagBits getMaxUsableSampleCount();

    void pickPhysicalDevice();

    void generateMipmaps(VkImage image, VkFormat imageFormat, int32_t texWidth, int32_t texHeight, uint32_t mipLevels);

    void createRenderPass();

    VkFormat findSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features);

    VkFormat findDepthFormat();

    void createCommandPool();

    void createSwapChain(android_app* pAndroidApp);

    void createFramebuffers();

    VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);

    VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);

    VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities, android_app* pAndroidApp);

    void recreateSwapChain(android_app* pAndroidApp);

    void cleanupSwapChain();

    void createImageViews();

    void createColorResources();

    void createDepthResources();

    void createSyncObjects();

    void cleanUp();

    void createCommandBuffers();

    void finalize();

    VkSampleCountFlagBits getMSAASamples() const;

    void createInstance();

    void setupDebugReportCallback();

public:
    VkSurfaceKHR mSurface_;
    VkDebugUtilsMessengerEXT mDebugMessenger_;
    VkDebugReportCallbackEXT mDebugReportCallback_;

    uint32_t mCurrentFrame_ = 0;
    VkSampleCountFlagBits mMSAASamples_ = VK_SAMPLE_COUNT_1_BIT;
    VkQueue mPresentQueue_;
    VkImage mColorImage_;
    VkDeviceMemory mColorImageMemory_;
    VkImageView mColorImageView_;
    VkImage mDepthImage_;
    VkDeviceMemory mDepthImageMemory_;
    VkImageView mDepthImageView_;
    VkSwapchainKHR mSwapChain_;
    std::vector<VkImage> mSwapChainImages_;
    VkExtent2D mSwapChainExtent_;
    std::vector<VkCommandBuffer> mCommandBuffers_;
    VkFormat mSwapChainImageFormat_;
    std::vector<VkImageView> mSwapChainImageViews_;
    std::vector<VkFramebuffer> mSwapChainFramebuffers_;
    std::vector<VkSemaphore> mImageAvailableSemaphores_;
    std::vector<VkSemaphore> mRenderFinishedSemaphores_;
    std::vector<VkFence> mInFlightFences_;
    std::unique_ptr<UniformBuffer> mCameraUniform_;

    std::vector<const char*> activeInstanceLayers{};
    std::vector<const char*> activeInstanceExtensions{};
};

} // namespace clay

#endif // CLAY_PLATFORM_ANDROID