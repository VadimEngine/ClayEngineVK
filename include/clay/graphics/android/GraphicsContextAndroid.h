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
        vk::SurfaceCapabilitiesKHR capabilities;
        std::vector<vk::SurfaceFormatKHR> formats;
        std::vector<vk::PresentModeKHR> presentModes;
    };

    static constexpr int MAX_FRAMES_IN_FLIGHT = 2;

    GraphicsContextAndroid(android_app* pAndroidApp);

    ~GraphicsContextAndroid();

    void initialize(android_app* pAndroidApp);

    void setupDebugMessenger();

    void setSurface(vk::SurfaceKHR surface);

    SwapChainSupportDetails querySwapChainSupport(vk::PhysicalDevice device);

    bool checkDeviceExtensionSupport(vk::PhysicalDevice device);

    bool isDeviceSuitable(vk::PhysicalDevice device);

    void createLogicalDevice();

    QueueFamilyIndices findQueueFamilies(vk::PhysicalDevice device);

    void populateDebugMessengerCreateInfo(vk::DebugUtilsMessengerCreateInfoEXT& createInfo);

    std::vector<const char*> getRequiredExtensions();

    bool checkValidationLayerSupport();

    void createDescriptorPool();

    vk::SampleCountFlagBits getMaxUsableSampleCount();

    void pickPhysicalDevice();

    void generateMipmaps(vk::Image image, vk::Format imageFormat, int32_t texWidth, int32_t texHeight, uint32_t mipLevels);

    void createRenderPass();

    vk::Format findSupportedFormat(const std::vector<vk::Format>& candidates, vk::ImageTiling tiling, vk::FormatFeatureFlags features);

    vk::Format findDepthFormat();

    void createCommandPool();

    void createSwapChain(android_app* pAndroidApp);

    void createFramebuffers();

    vk::SurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<vk::SurfaceFormatKHR>& availableFormats);

    vk::PresentModeKHR chooseSwapPresentMode(const std::vector<vk::PresentModeKHR>& availablePresentModes);

    vk::Extent2D chooseSwapExtent(const vk::SurfaceCapabilitiesKHR& capabilities, android_app* pAndroidApp);

    void recreateSwapChain(android_app* pAndroidApp);

    void cleanupSwapChain();

    void createImageViews();

    void createColorResources();

    void createDepthResources();

    void createSyncObjects();

    void cleanUp();

    void createCommandBuffers();

    void finalize();

    vk::SampleCountFlagBits getMSAASamples() const;

    void createInstance();

    void setupDebugReportCallback();

public:
    vk::SurfaceKHR mSurface_;
    vk::DebugUtilsMessengerEXT mDebugMessenger_;
    vk::DebugReportCallbackEXT mDebugReportCallback_;

    uint32_t mCurrentFrame_ = 0;
    vk::SampleCountFlagBits mMSAASamples_ = vk::SampleCountFlagBits::e1; //VK_SAMPLE_COUNT_1_BIT;
    vk::Queue mPresentQueue_;
    vk::Image mColorImage_;
    vk::DeviceMemory mColorImageMemory_;
    vk::ImageView mColorImageView_;
    vk::Image mDepthImage_;
    vk::DeviceMemory mDepthImageMemory_;
    vk::ImageView mDepthImageView_;
    vk::SwapchainKHR mSwapChain_;
    std::vector<vk::Image> mSwapChainImages_;
    vk::Extent2D mSwapChainExtent_;
    std::vector<vk::CommandBuffer> mCommandBuffers_;
    vk::Format mSwapChainImageFormat_;
    std::vector<vk::ImageView> mSwapChainImageViews_;
    std::vector<vk::Framebuffer> mSwapChainFramebuffers_;
    std::vector<vk::Semaphore> mImageAvailableSemaphores_;
    std::vector<vk::Semaphore> mRenderFinishedSemaphores_;
    std::vector<vk::Fence> mInFlightFences_;
    std::unique_ptr<UniformBuffer> mCameraUniform_;

    std::vector<const char*> activeInstanceLayers{};
    std::vector<const char*> activeInstanceExtensions{};
};

} // namespace clay

#endif // CLAY_PLATFORM_ANDROID