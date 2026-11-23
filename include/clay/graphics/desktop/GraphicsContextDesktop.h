#pragma once
#ifdef CLAY_PLATFORM_DESKTOP

// standard lib
#include <stdexcept>
#include <vector>
#include <array>
#include <optional>
#include <string>
// third party
#include <vulkan/vulkan.h>
// clay
#include "clay/graphics/common/BaseGraphicsContext.h"
#include "clay/graphics/common/UniformBuffer.h"
#include "clay/gui/desktop/Window.h"
#include "clay/utils/common/Utils.h"

namespace clay {

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

class GraphicsContextDesktop : public BaseGraphicsContext {
public:
    
    static constexpr int MAX_FRAMES_IN_FLIGHT = 2;

    GraphicsContextDesktop(Window& window);

    ~GraphicsContextDesktop();

    void initialize(Window& window);

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

    void createInstance();

    void createDescriptorPool();

    vk::SampleCountFlagBits getMaxUsableSampleCount();

    void pickPhysicalDevice();

    vk::ShaderModule createShaderModule(const utils::FileData& file);

    void generateMipmaps(vk::Image image, vk::Format imageFormat, int32_t texWidth, int32_t texHeight, uint32_t mipLevels);

    void createRenderPass();

    vk::Format findSupportedFormat(const std::vector<vk::Format>& candidates, vk::ImageTiling tiling, vk::FormatFeatureFlags features);

    vk::Format findDepthFormat();

    void createCommandPool();

    void createSwapChain(Window& mWindow);

    void createFramebuffers();

    vk::SurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<vk::SurfaceFormatKHR>& availableFormats);

    vk::PresentModeKHR chooseSwapPresentMode(const std::vector<vk::PresentModeKHR>& availablePresentModes, vk::PresentModeKHR desiredMode);

    vk::Extent2D chooseSwapExtent(const vk::SurfaceCapabilitiesKHR& capabilities, Window& window);

    void recreateSwapChain(Window& window);

    void cleanupSwapChain();

    void createImageViews();

    void createColorResources();

    void createDepthResources();

    // ---- Shadow mapping ----------------------------------------------------
    // Offscreen depth target rendered from the light's point of view. A project
    // creates a depth-only pipeline targeting getShadowRenderPass(), renders its
    // shadow casters into it (between beginShadowRenderPass / endShadowRenderPass),
    // then binds getShadowMapImageView()/Sampler() + the light uniform to its lit
    // materials and samples the map to shade. All optional: unused by default.
    static constexpr uint32_t SHADOW_MAP_SIZE = 2048;
    void createShadowResources();
    void beginShadowRenderPass(vk::CommandBuffer cmdBuffer);
    void endShadowRenderPass(vk::CommandBuffer cmdBuffer);
    vk::RenderPass getShadowRenderPass() const { return mShadowRenderPass_; }
    vk::ImageView  getShadowMapImageView() const { return mShadowImageView_; }
    vk::Sampler    getShadowMapSampler() const { return mShadowSampler_; }
    uint32_t       getShadowMapSize() const { return SHADOW_MAP_SIZE; }
    UniformBuffer* getLightUniform() const { return mLightUniform_.get(); }

    // --- Frame capture ---
    // Saves a finished frame (exactly what is presented, UI included) as a PNG, read
    // back from the swapchain image, so it works with the window hidden or covered.
    // requestCapture() queues it; AppDesktop::presentFrame takes it on the next frame.
    void requestCapture(const std::string& path) { mCapturePath_ = path; }
    bool capturePending() const { return !mCapturePath_.empty(); }
    // Number of captures finished so far (successful or not).
    uint32_t captureCount() const { return mCaptureCount_; }
    // Reads swapchain image `imageIndex` (which must be fully rendered and in the
    // present layout) into the pending capture path. Returns false on failure.
    bool captureSwapchainImage(uint32_t imageIndex);

    void createSyncObjects();

    void cleanUp();

    void createCommandBuffers();

    void finalize();

    vk::SampleCountFlagBits getMSAASamples() const;

    void setVSync(bool enabled);

public:
    vk::SurfaceKHR mSurface_;
    vk::DebugUtilsMessengerEXT mDebugMessenger_; // TODO debug might need to go in the app instead of graphics
    uint32_t mCurrentFrame_ = 0;
    vk::SampleCountFlagBits mMSAASamples_ = vk::SampleCountFlagBits::e1;
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
    std::unique_ptr<UniformBuffer> mCameraUniformHeadLocked_;

    // Shadow-map resources (see createShadowResources()).
    vk::Image        mShadowImage_;
    vk::DeviceMemory mShadowImageMemory_;
    vk::ImageView    mShadowImageView_;
    vk::Sampler      mShadowSampler_;
    vk::RenderPass   mShadowRenderPass_ = nullptr;
    vk::Framebuffer  mShadowFramebuffer_ = nullptr;
    vk::Format       mShadowDepthFormat_ = vk::Format::eD32Sfloat;
    std::unique_ptr<UniformBuffer> mLightUniform_; // light-space view*proj (mat4)

    std::string mCapturePath_;      // pending frame capture ("" = none)
    uint32_t mCaptureCount_ = 0;

    Window& mWindow_; // todo remove this
    bool mVSyncEnabled_;
};

} // namespace clay

#endif // CLAY_PLATFORM_DESKTOP