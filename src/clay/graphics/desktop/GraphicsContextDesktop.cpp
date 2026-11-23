#ifdef CLAY_PLATFORM_DESKTOP

// standard lib
#include <cstring>
#include <iostream>
#include <vector>
#include <set>
#include <algorithm>
// third party
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>
// clay
#include "clay/application/common/BaseScene.h"
// class
#include "clay/graphics/desktop/GraphicsContextDesktop.h"


namespace clay {

#ifdef NDEBUG
const bool enableValidationLayers = false;
#else
const bool enableValidationLayers = true;
#endif

const std::vector<const char*> validationLayers = {
    "VK_LAYER_KHRONOS_validation"
};

const std::vector<const char*> deviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

void DestroyDebugUtilsMessengerEXT(vk::Instance instance, vk::DebugUtilsMessengerEXT debugMessenger, const vk::AllocationCallbacks* pAllocator){
    auto func = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
        instance.getProcAddr("vkDestroyDebugUtilsMessengerEXT")
    );

    if (func) {
        func(static_cast<VkInstance>(instance), static_cast<VkDebugUtilsMessengerEXT>(debugMessenger), reinterpret_cast<const VkAllocationCallbacks*>(pAllocator));
    }
}

vk::Result CreateDebugUtilsMessengerEXT(
    vk::Instance instance,
    const vk::DebugUtilsMessengerCreateInfoEXT& createInfo,
    const vk::AllocationCallbacks* pAllocator,
    vk::DebugUtilsMessengerEXT& debugMessenger) {
    auto func = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
        instance.getProcAddr("vkCreateDebugUtilsMessengerEXT"));

    if (func != nullptr) {
        return static_cast<vk::Result>(
            func(static_cast<VkInstance>(instance),
                 reinterpret_cast<const VkDebugUtilsMessengerCreateInfoEXT*>(&createInfo),
                 reinterpret_cast<const VkAllocationCallbacks*>(pAllocator),
                 reinterpret_cast<VkDebugUtilsMessengerEXT*>(&debugMessenger)));
    } else {
        return vk::Result::eErrorExtensionNotPresent;
    }
}

VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
    vk::DebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    vk::DebugUtilsMessageTypeFlagsEXT messageType,
    const vk::DebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData) {
    using Sev = vk::DebugUtilsMessageSeverityFlagBitsEXT;
    using Type = vk::DebugUtilsMessageTypeFlagBitsEXT;
    // Performance messages are optimization hints, not correctness problems (e.g. the
    // expected "vertex attribute at location N not consumed by vertex shader" from
    // pipelines that bind a full vertex layout but whose shader reads a subset). Skip
    // them, and skip info/verbose chatter — surface only real warnings and errors.
    if (messageType & Type::ePerformance) return VK_FALSE;
    if (!(messageSeverity & (Sev::eWarning | Sev::eError))) return VK_FALSE;
    std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;
    return VK_FALSE;
}

GraphicsContextDesktop::GraphicsContextDesktop(Window& window)  
    : mWindow_(window),
      mVSyncEnabled_(false) {
    initialize(window);
}

GraphicsContextDesktop::~GraphicsContextDesktop() {}

void GraphicsContextDesktop::initialize(Window& window) {
    createInstance();
    setupDebugMessenger();
    setSurface(window.createSurface(mInstance_));
    mFrameDimensions_ = window.getDimensions();
    pickPhysicalDevice();
    createLogicalDevice();
    createSwapChain(window);
    createImageViews();
    createRenderPass();
    createCommandPool();
    createColorResources();
    createDepthResources();
    createFramebuffers();
    createDescriptorPool();
    createSyncObjects();
    createCommandBuffers();

    // create camera uniform
    mCameraUniform_ = std::make_unique<UniformBuffer>(
        *this,
        sizeof(clay::BaseScene::CameraConstant),
        nullptr
    );

    mCameraUniformHeadLocked_ = std::make_unique<UniformBuffer>(
        *this,
        sizeof(clay::BaseScene::CameraConstant),
        nullptr
    );

    createShadowResources();
}

bool GraphicsContextDesktop::checkValidationLayerSupport() {
    std::vector<vk::LayerProperties> availableLayers = vk::enumerateInstanceLayerProperties();

    for (const char* layerName : validationLayers) {
        bool layerFound = false;

        for (const auto& layerProperties : availableLayers) {
            if (strcmp(layerName, layerProperties.layerName) == 0) {
                layerFound = true;
                break;
            }
        }

        if (!layerFound) {
            return false;
        }
    }

    return true;
}

bool GraphicsContextDesktop::checkDeviceExtensionSupport(vk::PhysicalDevice device) {
    // Get available device extensions using Vulkan-Hpp
    std::vector<vk::ExtensionProperties> availableExtensions =
        device.enumerateDeviceExtensionProperties();

    std::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());

    for (const auto& extension : availableExtensions) {
        requiredExtensions.erase(extension.extensionName);
    }

    return requiredExtensions.empty();
}

void GraphicsContextDesktop::setSurface(vk::SurfaceKHR surface) {
    mSurface_ = surface;
}

SwapChainSupportDetails GraphicsContextDesktop::querySwapChainSupport(vk::PhysicalDevice device) {
    return {
        .capabilities = device.getSurfaceCapabilitiesKHR(mSurface_),
        .formats = device.getSurfaceFormatsKHR(mSurface_),
        .presentModes = device.getSurfacePresentModesKHR(mSurface_),
    };
}

void GraphicsContextDesktop::createLogicalDevice() {
    QueueFamilyIndices indices = findQueueFamilies(mPhysicalDevice_);

    std::vector<vk::DeviceQueueCreateInfo> queueCreateInfos;
    std::set<uint32_t> uniqueQueueFamilies = {indices.graphicsFamily.value(), indices.presentFamily.value()};

    float queuePriority = 1.0f;
    for (uint32_t queueFamily : uniqueQueueFamilies) {
        vk::DeviceQueueCreateInfo queueCreateInfo{
            .queueFamilyIndex = queueFamily,
            .queueCount = 1,
            .pQueuePriorities = &queuePriority
        };

        queueCreateInfos.push_back(queueCreateInfo);
    }

    vk::PhysicalDeviceFeatures supportedFeatures = mPhysicalDevice_.getFeatures();
    vk::PhysicalDeviceFeatures deviceFeatures{};
    deviceFeatures.samplerAnisotropy = vk::True;
    // Wireframe (VK_POLYGON_MODE_LINE) pipelines are allowed where the GPU supports them.
    deviceFeatures.fillModeNonSolid = supportedFeatures.fillModeNonSolid;

    vk::DeviceCreateInfo createInfo{
        .queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size()),
        .pQueueCreateInfos = queueCreateInfos.data(),
        .enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size()),
        .ppEnabledExtensionNames = deviceExtensions.data(),
        .pEnabledFeatures = &deviceFeatures,
    };

    if (enableValidationLayers) {
        createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
        createInfo.ppEnabledLayerNames = validationLayers.data();
    } else {
        createInfo.enabledLayerCount = 0;
    }

    mDevice_ = mPhysicalDevice_.createDevice(createInfo);

    mGraphicsQueue_ = mDevice_.getQueue(indices.graphicsFamily.value(), 0);
    mPresentQueue_  = mDevice_.getQueue(indices.presentFamily.value(), 0);
}

QueueFamilyIndices GraphicsContextDesktop::findQueueFamilies(vk::PhysicalDevice device) {
    QueueFamilyIndices indices;

    std::vector<vk::QueueFamilyProperties> queueFamilies = device.getQueueFamilyProperties();

    for (uint32_t i = 0; i < static_cast<uint32_t>(queueFamilies.size()); ++i) {
        const auto& queueFamily = queueFamilies[i];

        if (queueFamily.queueFlags & vk::QueueFlagBits::eGraphics) {
            indices.graphicsFamily = i;
        }

        if (device.getSurfaceSupportKHR(i, mSurface_)) {
            indices.presentFamily = i;
        }

        if (indices.isComplete()) {
            break;
        }
    }

    return indices;
}

bool GraphicsContextDesktop::isDeviceSuitable(vk::PhysicalDevice device) {
    QueueFamilyIndices indices = findQueueFamilies(device);

    bool extensionsSupported = checkDeviceExtensionSupport(device);

    bool swapChainAdequate = false;
    if (extensionsSupported) {
        SwapChainSupportDetails swapChainSupport = querySwapChainSupport(device);
        swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
    }
    vk::PhysicalDeviceFeatures     supportedFeatures = device.getFeatures();

    return indices.isComplete() && extensionsSupported && swapChainAdequate && supportedFeatures.samplerAnisotropy;
}

vk::SampleCountFlagBits GraphicsContextDesktop::getMaxUsableSampleCount() {
    vk::PhysicalDeviceProperties physicalDeviceProperties = mPhysicalDevice_.getProperties();

    vk::SampleCountFlags counts =
        physicalDeviceProperties.limits.framebufferColorSampleCounts &
        physicalDeviceProperties.limits.framebufferDepthSampleCounts;

    if (counts & vk::SampleCountFlagBits::e64) return vk::SampleCountFlagBits::e64;
    if (counts & vk::SampleCountFlagBits::e32) return vk::SampleCountFlagBits::e32;
    if (counts & vk::SampleCountFlagBits::e16) return vk::SampleCountFlagBits::e16;
    if (counts & vk::SampleCountFlagBits::e8)  return vk::SampleCountFlagBits::e8;
    if (counts & vk::SampleCountFlagBits::e4)  return vk::SampleCountFlagBits::e4;
    if (counts & vk::SampleCountFlagBits::e2)  return vk::SampleCountFlagBits::e2;

    return vk::SampleCountFlagBits::e1;
}

void GraphicsContextDesktop::pickPhysicalDevice() {
    std::vector<vk::PhysicalDevice> devices = mInstance_.enumeratePhysicalDevices();

    for (const auto& device : devices) {
        if (isDeviceSuitable(device)) {
            mPhysicalDevice_ = device;
            //mMSAASamples_ = getMaxUsableSampleCount();
            mMSAASamples_ = vk::SampleCountFlagBits::e1;
            break;
        }
    }

    if (mPhysicalDevice_ == nullptr) {
        throw std::runtime_error("failed to find a suitable GPU!");
    }
}

std::vector<const char*> GraphicsContextDesktop::getRequiredExtensions() {
    uint32_t glfwExtensionCount = 0;
    const char** glfwExtensions;
    glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

    if (enableValidationLayers) {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    return extensions;
}

void GraphicsContextDesktop::setupDebugMessenger() {
    if (!enableValidationLayers) return;

    vk::DebugUtilsMessengerCreateInfoEXT createInfo;
    populateDebugMessengerCreateInfo(createInfo);

    if (CreateDebugUtilsMessengerEXT(mInstance_, createInfo, nullptr, mDebugMessenger_) != vk::Result::eSuccess) {
        throw std::runtime_error("failed to set up debug messenger!");
    }
}

void GraphicsContextDesktop::populateDebugMessengerCreateInfo(vk::DebugUtilsMessengerCreateInfoEXT& createInfo) {
    createInfo = vk::DebugUtilsMessengerCreateInfoEXT{
        .messageSeverity = vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning | vk::DebugUtilsMessageSeverityFlagBitsEXT::eError,
        .messageType = vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral | vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation | vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance,
        .pfnUserCallback = reinterpret_cast<PFN_vkDebugUtilsMessengerCallbackEXT>(debugCallback),
        .pUserData = nullptr
    };
}

void GraphicsContextDesktop::createInstance() {
    if (enableValidationLayers && !checkValidationLayerSupport()) {
        throw std::runtime_error("validation layers requested, but not available!");
    }

    vk::ApplicationInfo appInfo{
        .pApplicationName = "ClayEngine Desktop Demo",
        .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
        .pEngineName = "Clay",
        .engineVersion = VK_MAKE_VERSION(1, 0, 0),
        .apiVersion = VK_API_VERSION_1_0
    };

    auto extensions = getRequiredExtensions();
    vk::InstanceCreateInfo createInfo{
        .pApplicationInfo = &appInfo,
        .enabledExtensionCount = static_cast<uint32_t>(extensions.size()),
        .ppEnabledExtensionNames = extensions.data(),
    };


    vk::DebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
    if (enableValidationLayers) {
        createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
        createInfo.ppEnabledLayerNames = validationLayers.data();

        populateDebugMessengerCreateInfo(debugCreateInfo);
        createInfo.pNext = (vk::DebugUtilsMessengerCreateInfoEXT*) &debugCreateInfo;
    } else {
        createInfo.enabledLayerCount = 0;
        createInfo.pNext = nullptr;
    }

    vk::Result result = vk::createInstance(&createInfo, nullptr, &mInstance_);
    if (result != vk::Result::eSuccess) {
        throw std::runtime_error("failed to create instance!");
    }
}

void GraphicsContextDesktop::createDescriptorPool() {
    uint32_t maxSets = 1024;
    std::vector<vk::DescriptorPoolSize> poolSizes = {
        { vk::DescriptorType::eSampler, 16 * maxSets },
        { vk::DescriptorType::eSampledImage, 16 * maxSets },
        { vk::DescriptorType::eStorageImage, 16 * maxSets },
        { vk::DescriptorType::eUniformBuffer, 16 * maxSets },
        { vk::DescriptorType::eStorageBuffer, 16 * maxSets },
        { vk::DescriptorType::eCombinedImageSampler, 16 * maxSets },
    };
    vk::DescriptorPoolCreateInfo poolInfo{
        .pNext = nullptr,
        .flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
        .maxSets = maxSets,
        .poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
        .pPoolSizes = poolSizes.data()
    };

    mDescriptorPool_ = mDevice_.createDescriptorPool(poolInfo);
}

void GraphicsContextDesktop::generateMipmaps(vk::Image image, vk::Format imageFormat, int32_t texWidth, int32_t texHeight, uint32_t mipLevels) {
    // Check if image format supports linear blitting
    vk::FormatProperties formatProperties = mPhysicalDevice_.getFormatProperties(imageFormat);

    if (!(formatProperties.optimalTilingFeatures & vk::FormatFeatureFlagBits::eSampledImageFilterLinear)) {
        throw std::runtime_error("texture image format does not support linear blitting!");
    }

    vk::CommandBuffer commandBuffer = beginSingleTimeCommands();

    vk::ImageMemoryBarrier barrier{
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = image,
        .subresourceRange = {
            .aspectMask =  vk::ImageAspectFlagBits::eColor,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1,
        }
    };

    int32_t mipWidth = texWidth;
    int32_t mipHeight = texHeight;

    for (uint32_t i = 1; i < mipLevels; i++) {
        barrier.subresourceRange.baseMipLevel = i - 1;
        barrier.oldLayout = vk::ImageLayout::eTransferDstOptimal;
        barrier.newLayout = vk::ImageLayout::eTransferSrcOptimal;
        barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
        barrier.dstAccessMask = vk::AccessFlagBits::eTransferRead;

        commandBuffer.pipelineBarrier(
            vk::PipelineStageFlagBits::eTransfer,
            vk::PipelineStageFlagBits::eTransfer,
            {},
            nullptr,
            nullptr,
            barrier
        );

        vk::ImageBlit blit{
            .srcSubresource = {
                .aspectMask = vk::ImageAspectFlagBits::eColor,
                .mipLevel = i - 1,
                .baseArrayLayer = 0,
                .layerCount = 1
            },
            .srcOffsets = vk::ArrayWrapper1D<vk::Offset3D, 2>{
                std::array<vk::Offset3D, 2>{{
                    {0, 0, 0},
                    {mipWidth, mipHeight, 1}
                }}
            },
            .dstSubresource = {
                .aspectMask = vk::ImageAspectFlagBits::eColor,
                .mipLevel = i,
                .baseArrayLayer = 0,
                .layerCount = 1
            },
            .dstOffsets = vk::ArrayWrapper1D<vk::Offset3D, 2>{
                std::array<vk::Offset3D, 2>{{
                    {0, 0, 0},
                    {mipWidth > 1 ? mipWidth / 2 : 1, mipHeight > 1 ? mipHeight / 2 : 1, 1}
                }}
            },
        };

        commandBuffer.blitImage(
            image, vk::ImageLayout::eTransferSrcOptimal,
            image, vk::ImageLayout::eTransferDstOptimal,
            { blit },
            vk::Filter::eLinear
        );

        barrier.oldLayout = vk::ImageLayout::eTransferSrcOptimal,
        barrier.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
        barrier.srcAccessMask = vk::AccessFlagBits::eTransferRead;
        barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

        commandBuffer.pipelineBarrier(
            vk::PipelineStageFlagBits::eTransfer,
            vk::PipelineStageFlagBits::eFragmentShader,
            {},
            nullptr,
            nullptr,
            barrier
        );

        if (mipWidth > 1) mipWidth /= 2;
        if (mipHeight > 1) mipHeight /= 2;
    }

    barrier.subresourceRange.baseMipLevel = mipLevels - 1;
    barrier.oldLayout = vk::ImageLayout::eTransferSrcOptimal;
    barrier.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
    barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
    barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

    commandBuffer.pipelineBarrier(
        vk::PipelineStageFlagBits::eTransfer,
        vk::PipelineStageFlagBits::eFragmentShader,
        {},
        nullptr,
        nullptr,
        barrier
    );

    endSingleTimeCommands(commandBuffer);
}

vk::Format GraphicsContextDesktop::findSupportedFormat(const std::vector<vk::Format>& candidates, vk::ImageTiling tiling, vk::FormatFeatureFlags features) {
    for (vk::Format format : candidates) {
        vk::FormatProperties props = mPhysicalDevice_.getFormatProperties(format);

        if (tiling == vk::ImageTiling::eLinear && (props.linearTilingFeatures & features) == features) {
            return format;
        } else if (tiling == vk::ImageTiling::eOptimal && (props.optimalTilingFeatures & features) == features) {
            return format;
        }
    }

    throw std::runtime_error("failed to find supported format!");
}

// True if the depth format also carries a stencil component.
static bool depthFormatHasStencil(vk::Format format) {
    return format == vk::Format::eD32SfloatS8Uint
        || format == vk::Format::eD24UnormS8Uint
        || format == vk::Format::eD16UnormS8Uint;
}

vk::Format GraphicsContextDesktop::findDepthFormat() {
    // Prefer formats that include a STENCIL component so effects like stencil
    // outlines work; fall back to depth-only if the device supports neither.
    return findSupportedFormat(
        {
            vk::Format::eD32SfloatS8Uint,
            vk::Format::eD24UnormS8Uint,
            vk::Format::eD32Sfloat
        },
        vk::ImageTiling::eOptimal,
        vk::FormatFeatureFlagBits::eDepthStencilAttachment
    );
}

void GraphicsContextDesktop::createRenderPass() {
    vk::AttachmentDescription colorAttachment{
        .format = mSwapChainImageFormat_,
        .samples = mMSAASamples_,
        .loadOp = vk::AttachmentLoadOp::eClear,
        .storeOp = vk::AttachmentStoreOp::eStore,
        .stencilLoadOp = vk::AttachmentLoadOp::eDontCare,
        .stencilStoreOp = vk::AttachmentStoreOp::eDontCare,
        .initialLayout = vk::ImageLayout::eUndefined,
        .finalLayout = (mMSAASamples_ == vk::SampleCountFlagBits::e1)
            ? vk::ImageLayout::ePresentSrcKHR
            : vk::ImageLayout::eColorAttachmentOptimal,
    };

    vk::AttachmentDescription depthAttachment{
        .format = findDepthFormat(),
        .samples = mMSAASamples_,
        .loadOp = vk::AttachmentLoadOp::eClear,
        .storeOp = vk::AttachmentStoreOp::eDontCare,
        .stencilLoadOp = vk::AttachmentLoadOp::eClear,   // clear stencil each frame (for stencil outlines)
        .stencilStoreOp = vk::AttachmentStoreOp::eDontCare,
        .initialLayout = vk::ImageLayout::eUndefined,
        .finalLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal,
    };

    vk::AttachmentDescription colorAttachmentResolve{
        .format = mSwapChainImageFormat_,
        .samples = vk::SampleCountFlagBits::e1,
        .loadOp = vk::AttachmentLoadOp::eDontCare,
        .storeOp = vk::AttachmentStoreOp::eStore,
        .stencilLoadOp = vk::AttachmentLoadOp::eDontCare,
        .stencilStoreOp = vk::AttachmentStoreOp::eDontCare,
        .initialLayout = vk::ImageLayout::eUndefined,
        .finalLayout = vk::ImageLayout::ePresentSrcKHR,
    };

    vk::AttachmentReference colorAttachmentRef{
        .attachment = 0,
        .layout = vk::ImageLayout::eColorAttachmentOptimal
    };

    vk::AttachmentReference depthAttachmentRef{
       .attachment = 1,
       .layout = vk::ImageLayout::eDepthStencilAttachmentOptimal
    };

    vk::AttachmentReference colorAttachmentResolveRef{
        .attachment = 2,
        .layout = vk::ImageLayout::eColorAttachmentOptimal
    };

    vk::SubpassDescription subpass{
        .pipelineBindPoint = vk::PipelineBindPoint::eGraphics,
        .colorAttachmentCount = 1,
        .pColorAttachments = &colorAttachmentRef,
        .pDepthStencilAttachment = &depthAttachmentRef,
    };
    if (mMSAASamples_ > vk::SampleCountFlagBits::e1) {
        subpass.pResolveAttachments = &colorAttachmentResolveRef;
    }

    vk::SubpassDependency dependency{
        .srcSubpass = VK_SUBPASS_EXTERNAL,
        .dstSubpass = 0,
        .srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput | vk::PipelineStageFlagBits::eEarlyFragmentTests,
        .dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput | vk::PipelineStageFlagBits::eEarlyFragmentTests,
        .srcAccessMask = {},
        .dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite | vk::AccessFlagBits::eDepthStencilAttachmentWrite,
    };

    std::vector<vk::AttachmentDescription> attachments;
    if (mMSAASamples_ > vk::SampleCountFlagBits::e1) {
        attachments = { colorAttachment, depthAttachment, colorAttachmentResolve };
    } else {
        attachments = { colorAttachment, depthAttachment };
    }
    vk::RenderPassCreateInfo renderPassInfo{
        .attachmentCount = static_cast<uint32_t>(attachments.size()),
        .pAttachments = attachments.data(),
        .subpassCount = 1,
        .pSubpasses = &subpass,
        .dependencyCount = 1,
        .pDependencies = &dependency,
    };

    mRenderPass_ = mDevice_.createRenderPass(renderPassInfo);
}

void GraphicsContextDesktop::createCommandPool() {
    QueueFamilyIndices queueFamilyIndices = findQueueFamilies(mPhysicalDevice_);

    vk::CommandPoolCreateInfo poolInfo{
        .flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
        .queueFamilyIndex = queueFamilyIndices.graphicsFamily.value(),
    };

    mCommandPool_ = mDevice_.createCommandPool(poolInfo);
}

void GraphicsContextDesktop::createSwapChain(Window& mWindow) {
    SwapChainSupportDetails swapChainSupport = querySwapChainSupport(mPhysicalDevice_);

    vk::SurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
    vk::PresentModeKHR presentMode = vk::PresentModeKHR::eFifo;
    if (!mVSyncEnabled_) {
        presentMode = chooseSwapPresentMode(swapChainSupport.presentModes, vk::PresentModeKHR::eMailbox);
    }
    vk::Extent2D extent = chooseSwapExtent(swapChainSupport.capabilities, mWindow);

    uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
    if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount) {
        imageCount = swapChainSupport.capabilities.maxImageCount;
    }

    vk::SwapchainCreateInfoKHR createInfo{
        .surface = mSurface_,
        .minImageCount = imageCount,
        .imageFormat = surfaceFormat.format,
        .imageColorSpace = surfaceFormat.colorSpace,
        .imageExtent = extent,
        .imageArrayLayers = 1,
        // TransferSrc so a frame can be read back (captureSwapchainImage).
        .imageUsage =  vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferSrc,
    };

    QueueFamilyIndices indices = findQueueFamilies(mPhysicalDevice_);
    uint32_t queueFamilyIndices[] = {indices.graphicsFamily.value(), indices.presentFamily.value()};

    if (indices.graphicsFamily != indices.presentFamily) {
        createInfo.imageSharingMode = vk::SharingMode::eConcurrent;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = queueFamilyIndices;
    } else {
        createInfo.imageSharingMode = vk::SharingMode::eExclusive;
    }

    createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
    createInfo.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;
    createInfo.presentMode = presentMode;
    createInfo.clipped = vk::True;
    createInfo.oldSwapchain = nullptr;

    mSwapChain_ = mDevice_.createSwapchainKHR(createInfo);
    mSwapChainImages_ = mDevice_.getSwapchainImagesKHR(mSwapChain_);
    mSwapChainImageFormat_ = surfaceFormat.format;
    mSwapChainExtent_ = extent;
}

vk::SurfaceFormatKHR GraphicsContextDesktop::chooseSwapSurfaceFormat(const std::vector<vk::SurfaceFormatKHR>& availableFormats) {
    for (const auto& availableFormat : availableFormats) {
        // eB8G8R8A8Unorm (this is related to gamma correction)
        if (availableFormat.format == vk::Format::eR8G8B8A8Srgb && availableFormat.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear) {
            return availableFormat;
        }
    }

    return availableFormats[0];
}

void GraphicsContextDesktop::createFramebuffers() {
    mSwapChainFramebuffers_.resize(mSwapChainImageViews_.size());

    for (size_t i = 0; i < mSwapChainImageViews_.size(); i++) {
        std::vector<vk::ImageView> attachments;

        if (mMSAASamples_ > vk::SampleCountFlagBits::e1) {
            attachments = {
                mColorImageView_,
                mDepthImageView_,
                mSwapChainImageViews_[i] // resolve attachment
            };
        } else {
            attachments = {
                mSwapChainImageViews_[i], // directly as color attachment
                mDepthImageView_
            };
        }

        vk::FramebufferCreateInfo framebufferInfo{
            .renderPass = mRenderPass_,
            .attachmentCount = static_cast<uint32_t>(attachments.size()),
            .pAttachments = attachments.data(),
            .width = mSwapChainExtent_.width,
            .height = mSwapChainExtent_.height,
            .layers = 1,
        };

        mSwapChainFramebuffers_[i] = mDevice_.createFramebuffer(framebufferInfo);
    }
}

vk::PresentModeKHR GraphicsContextDesktop::chooseSwapPresentMode(const std::vector<vk::PresentModeKHR>& availablePresentModes, vk::PresentModeKHR desiredMode) {
    for (const auto& availablePresentMode : availablePresentModes) {
        if (availablePresentMode == desiredMode) {
            return availablePresentMode;
        }
    }

    return vk::PresentModeKHR::eFifo;
}

vk::Extent2D GraphicsContextDesktop::chooseSwapExtent(const vk::SurfaceCapabilitiesKHR& capabilities, Window& window) {
    if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
        return capabilities.currentExtent;
    } else {
        auto [width, height] = window.getDimensions();

        vk::Extent2D actualExtent = {
            static_cast<uint32_t>(width),
            static_cast<uint32_t>(height)
        };

        actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
        actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

        return actualExtent;
    }
}

// TODO fix this to not take in window but only the needed information
void GraphicsContextDesktop::recreateSwapChain(Window& window) {
    auto [width, height] = window.getDimensions();

    while (width == 0 || height == 0) {
        std::tie(width, height) = window.getDimensions();
        glfwWaitEvents();
    }

    mDevice_.waitIdle();

    cleanupSwapChain();

    createSwapChain(window);
    // The frame is now this size: everything that lays itself out or maps the pointer by
    // getFrameDimensions() (the UI view, picking, 2D overlays) must see the new one.
    // (Left at the start-up size, the UI stayed laid out for the old window after a
    // resize / maximise, so the pointer no longer lined up with its buttons.)
    mFrameDimensions_ = { static_cast<int>(mSwapChainExtent_.width), static_cast<int>(mSwapChainExtent_.height) };
    createImageViews();
    createColorResources();
    createDepthResources();
    createFramebuffers();
}

void GraphicsContextDesktop::cleanupSwapChain() {
    mDevice_.destroyImageView(mDepthImageView_);
    mDevice_.destroyImage(mDepthImage_);
    mDevice_.freeMemory(mDepthImageMemory_);

    mDevice_.destroyImageView(mColorImageView_);
    mDevice_.destroyImage(mColorImage_);
    mDevice_.freeMemory(mColorImageMemory_);

    for (const auto& framebuffer : mSwapChainFramebuffers_) {
        mDevice_.destroyFramebuffer(framebuffer);
    }

    for (const auto& imageView : mSwapChainImageViews_) {
        mDevice_.destroyImageView(imageView);
    }

    mDevice_.destroySwapchainKHR(mSwapChain_);
}

void GraphicsContextDesktop::createImageViews() {
    mSwapChainImageViews_.resize(mSwapChainImages_.size());

    for (uint32_t i = 0; i < mSwapChainImages_.size(); i++) {
        mSwapChainImageViews_[i] = createImageView(mSwapChainImages_[i], mSwapChainImageFormat_, vk::ImageAspectFlagBits::eColor, 1);
    }
}

void GraphicsContextDesktop::createColorResources() {
    vk::Format colorFormat = mSwapChainImageFormat_;

    createImage(
        mSwapChainExtent_.width,
        mSwapChainExtent_.height,
        1,
        mMSAASamples_,
        colorFormat,
        vk::ImageTiling::eOptimal,
        vk::ImageUsageFlagBits::eTransientAttachment | vk::ImageUsageFlagBits::eColorAttachment,
        vk::MemoryPropertyFlagBits::eDeviceLocal,
        mColorImage_,
        mColorImageMemory_
    );

    mColorImageView_ = createImageView(mColorImage_, colorFormat, vk::ImageAspectFlagBits::eColor, 1);
}

void GraphicsContextDesktop::createDepthResources() {
    vk::Format depthFormat = findDepthFormat();

    createImage(
        mSwapChainExtent_.width,
        mSwapChainExtent_.height,
        1,
        mMSAASamples_,
        depthFormat,
        vk::ImageTiling::eOptimal,
        vk::ImageUsageFlagBits::eDepthStencilAttachment, 
        vk::MemoryPropertyFlagBits::eDeviceLocal,
        mDepthImage_,
        mDepthImageMemory_
    );
    
    vk::ImageAspectFlags aspect = vk::ImageAspectFlagBits::eDepth;
    if (depthFormatHasStencil(depthFormat)) aspect |= vk::ImageAspectFlagBits::eStencil;
    mDepthImageView_ = createImageView(mDepthImage_, depthFormat, aspect, 1);
}

void GraphicsContextDesktop::createShadowResources() {
    // Depth format for the shadow map (must be usable as a sampled image).
    mShadowDepthFormat_ = findSupportedFormat(
        { vk::Format::eD32Sfloat, vk::Format::eD16Unorm },
        vk::ImageTiling::eOptimal,
        vk::FormatFeatureFlagBits::eDepthStencilAttachment | vk::FormatFeatureFlagBits::eSampledImage);

    createImage(
        SHADOW_MAP_SIZE, SHADOW_MAP_SIZE, 1,
        vk::SampleCountFlagBits::e1,
        mShadowDepthFormat_,
        vk::ImageTiling::eOptimal,
        vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eSampled,
        vk::MemoryPropertyFlagBits::eDeviceLocal,
        mShadowImage_, mShadowImageMemory_);

    mShadowImageView_ = createImageView(mShadowImage_, mShadowDepthFormat_,
                                        vk::ImageAspectFlagBits::eDepth, 1);

    // Clamp to a white border so anything outside the light's view reads as "lit".
    vk::SamplerCreateInfo samplerInfo{
        .magFilter = vk::Filter::eLinear,
        .minFilter = vk::Filter::eLinear,
        .mipmapMode = vk::SamplerMipmapMode::eNearest,
        .addressModeU = vk::SamplerAddressMode::eClampToBorder,
        .addressModeV = vk::SamplerAddressMode::eClampToBorder,
        .addressModeW = vk::SamplerAddressMode::eClampToBorder,
        .borderColor = vk::BorderColor::eFloatOpaqueWhite,
    };
    mShadowSampler_ = mDevice_.createSampler(samplerInfo);

    // Depth-only render pass that leaves the map in a shader-readable layout.
    vk::AttachmentDescription depthAttachment{
        .format = mShadowDepthFormat_,
        .samples = vk::SampleCountFlagBits::e1,
        .loadOp = vk::AttachmentLoadOp::eClear,
        .storeOp = vk::AttachmentStoreOp::eStore,
        .stencilLoadOp = vk::AttachmentLoadOp::eDontCare,
        .stencilStoreOp = vk::AttachmentStoreOp::eDontCare,
        .initialLayout = vk::ImageLayout::eUndefined,
        .finalLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
    };
    vk::AttachmentReference depthRef{
        .attachment = 0,
        .layout = vk::ImageLayout::eDepthStencilAttachmentOptimal,
    };
    vk::SubpassDescription subpass{
        .pipelineBindPoint = vk::PipelineBindPoint::eGraphics,
        .colorAttachmentCount = 0,
        .pDepthStencilAttachment = &depthRef,
    };
    // Serialise the undefined->attachment write and the write->sample transitions.
    std::array<vk::SubpassDependency, 2> deps{};
    deps[0] = vk::SubpassDependency{
        .srcSubpass = VK_SUBPASS_EXTERNAL,
        .dstSubpass = 0,
        .srcStageMask = vk::PipelineStageFlagBits::eFragmentShader,
        .dstStageMask = vk::PipelineStageFlagBits::eEarlyFragmentTests,
        .srcAccessMask = vk::AccessFlagBits::eShaderRead,
        .dstAccessMask = vk::AccessFlagBits::eDepthStencilAttachmentWrite,
        .dependencyFlags = vk::DependencyFlagBits::eByRegion,
    };
    deps[1] = vk::SubpassDependency{
        .srcSubpass = 0,
        .dstSubpass = VK_SUBPASS_EXTERNAL,
        .srcStageMask = vk::PipelineStageFlagBits::eLateFragmentTests,
        .dstStageMask = vk::PipelineStageFlagBits::eFragmentShader,
        .srcAccessMask = vk::AccessFlagBits::eDepthStencilAttachmentWrite,
        .dstAccessMask = vk::AccessFlagBits::eShaderRead,
        .dependencyFlags = vk::DependencyFlagBits::eByRegion,
    };
    vk::RenderPassCreateInfo rpInfo{
        .attachmentCount = 1,
        .pAttachments = &depthAttachment,
        .subpassCount = 1,
        .pSubpasses = &subpass,
        .dependencyCount = static_cast<uint32_t>(deps.size()),
        .pDependencies = deps.data(),
    };
    mShadowRenderPass_ = mDevice_.createRenderPass(rpInfo);

    vk::FramebufferCreateInfo fbInfo{
        .renderPass = mShadowRenderPass_,
        .attachmentCount = 1,
        .pAttachments = &mShadowImageView_,
        .width = SHADOW_MAP_SIZE,
        .height = SHADOW_MAP_SIZE,
        .layers = 1,
    };
    mShadowFramebuffer_ = mDevice_.createFramebuffer(fbInfo);

    // Light-space view*proj matrix, set by the scene each frame.
    mLightUniform_ = std::make_unique<UniformBuffer>(*this, sizeof(glm::mat4), nullptr);
}

void GraphicsContextDesktop::beginShadowRenderPass(vk::CommandBuffer cmdBuffer) {
    vk::ClearValue clear{};
    clear.depthStencil = vk::ClearDepthStencilValue(1.0f, 0);

    vk::RenderPassBeginInfo info{
        .renderPass = mShadowRenderPass_,
        .framebuffer = mShadowFramebuffer_,
        .renderArea = { .offset = {0, 0}, .extent = { SHADOW_MAP_SIZE, SHADOW_MAP_SIZE } },
        .clearValueCount = 1,
        .pClearValues = &clear,
    };
    cmdBuffer.beginRenderPass(info, vk::SubpassContents::eInline);

    vk::Viewport vp{};
    vp.x = 0.0f; vp.y = 0.0f;
    vp.width = static_cast<float>(SHADOW_MAP_SIZE);
    vp.height = static_cast<float>(SHADOW_MAP_SIZE);
    vp.minDepth = 0.0f; vp.maxDepth = 1.0f;
    cmdBuffer.setViewport(0, 1, &vp);

    vk::Rect2D sc{ .offset = {0, 0}, .extent = { SHADOW_MAP_SIZE, SHADOW_MAP_SIZE } };
    cmdBuffer.setScissor(0, 1, &sc);
}

void GraphicsContextDesktop::endShadowRenderPass(vk::CommandBuffer cmdBuffer) {
    cmdBuffer.endRenderPass();
}

bool GraphicsContextDesktop::captureSwapchainImage(uint32_t imageIndex) {
    const std::string path = mCapturePath_;
    mCapturePath_.clear();
    ++mCaptureCount_;
    if (path.empty() || imageIndex >= mSwapChainImages_.size()) return false;

    const uint32_t width = mSwapChainExtent_.width, height = mSwapChainExtent_.height;
    const vk::DeviceSize size = static_cast<vk::DeviceSize>(width) * height * 4;
    vk::Buffer buffer;
    vk::DeviceMemory memory;
    createBuffer(size, vk::BufferUsageFlagBits::eTransferDst,
                 vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
                 buffer, memory);

    const vk::Image image = mSwapChainImages_[imageIndex];
    const vk::ImageSubresourceRange range{ vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 };
    auto barrier = [&](vk::CommandBuffer cmd, vk::ImageLayout from, vk::ImageLayout to,
                       vk::AccessFlags srcAccess, vk::AccessFlags dstAccess) {
        vk::ImageMemoryBarrier b{
            .srcAccessMask = srcAccess,
            .dstAccessMask = dstAccess,
            .oldLayout = from,
            .newLayout = to,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = image,
            .subresourceRange = range,
        };
        cmd.pipelineBarrier(vk::PipelineStageFlagBits::eAllCommands, vk::PipelineStageFlagBits::eAllCommands,
                            {}, nullptr, nullptr, b);
    };

    vk::CommandBuffer cmd = beginSingleTimeCommands();
    barrier(cmd, vk::ImageLayout::ePresentSrcKHR, vk::ImageLayout::eTransferSrcOptimal,
            vk::AccessFlagBits::eMemoryRead, vk::AccessFlagBits::eTransferRead);
    vk::BufferImageCopy region{
        .bufferOffset = 0,
        .imageSubresource = { vk::ImageAspectFlagBits::eColor, 0, 0, 1 },
        .imageOffset = vk::Offset3D{0, 0, 0},
        .imageExtent = vk::Extent3D{width, height, 1},
    };
    cmd.copyImageToBuffer(image, vk::ImageLayout::eTransferSrcOptimal, buffer, region);
    barrier(cmd, vk::ImageLayout::eTransferSrcOptimal, vk::ImageLayout::ePresentSrcKHR,
            vk::AccessFlagBits::eTransferRead, vk::AccessFlagBits::eMemoryRead);
    endSingleTimeCommands(cmd);   // submits and waits

    // Copy out in one pass before touching the pixels: mapped GPU memory is typically
    // write-combined, and reading it piecemeal (as the PNG encoder would) takes seconds.
    std::vector<unsigned char> pixels(static_cast<size_t>(size));
    std::memcpy(pixels.data(), mDevice_.mapMemory(memory, 0, size), pixels.size());
    const bool bgra = mSwapChainImageFormat_ == vk::Format::eB8G8R8A8Srgb
                   || mSwapChainImageFormat_ == vk::Format::eB8G8R8A8Unorm;
    for (size_t i = 0; i < pixels.size(); i += 4) {
        if (bgra) std::swap(pixels[i], pixels[i + 2]);
        pixels[i + 3] = 255;   // the window is opaque whatever alpha the frame left behind
    }
    const bool ok = stbi_write_png(path.c_str(), static_cast<int>(width), static_cast<int>(height),
                                   4, pixels.data(), static_cast<int>(width) * 4) != 0;
    mDevice_.unmapMemory(memory);
    mDevice_.destroyBuffer(buffer);
    mDevice_.freeMemory(memory);
    return ok;
}

void GraphicsContextDesktop::createSyncObjects() {
    mImageAvailableSemaphores_.resize(GraphicsContextDesktop::MAX_FRAMES_IN_FLIGHT);
    mRenderFinishedSemaphores_.resize(GraphicsContextDesktop::MAX_FRAMES_IN_FLIGHT);
    mInFlightFences_.resize(GraphicsContextDesktop::MAX_FRAMES_IN_FLIGHT);

    vk::SemaphoreCreateInfo semaphoreInfo{};

    vk::FenceCreateInfo fenceInfo{
        .flags = vk::FenceCreateFlagBits::eSignaled
    };

    for (size_t i = 0; i < GraphicsContextDesktop::MAX_FRAMES_IN_FLIGHT; i++) {
        try {
            mImageAvailableSemaphores_[i] = mDevice_.createSemaphore(semaphoreInfo);
            mRenderFinishedSemaphores_[i] = mDevice_.createSemaphore(semaphoreInfo);
            mInFlightFences_[i] = mDevice_.createFence(fenceInfo);
        } catch (const vk::SystemError& err) {
            throw std::runtime_error("failed to create synchronization objects for a frame!");
        }
    }
}

void GraphicsContextDesktop::cleanUp() {
    cleanupSwapChain();
    mCameraUniform_.reset();
    mCameraUniformHeadLocked_.reset();

    // Shadow-map resources.
    mLightUniform_.reset();
    if (mShadowFramebuffer_) mDevice_.destroyFramebuffer(mShadowFramebuffer_);
    if (mShadowRenderPass_)  mDevice_.destroyRenderPass(mShadowRenderPass_);
    if (mShadowSampler_)     mDevice_.destroySampler(mShadowSampler_);
    if (mShadowImageView_)   mDevice_.destroyImageView(mShadowImageView_);
    if (mShadowImage_)       mDevice_.destroyImage(mShadowImage_);
    if (mShadowImageMemory_) mDevice_.freeMemory(mShadowImageMemory_);

    mDevice_.destroyRenderPass(mRenderPass_);

    mDevice_.destroyDescriptorPool(mDescriptorPool_);

    for (size_t i = 0; i < GraphicsContextDesktop::MAX_FRAMES_IN_FLIGHT; ++i) {
        mDevice_.destroySemaphore(mRenderFinishedSemaphores_[i]);
        mDevice_.destroySemaphore(mImageAvailableSemaphores_[i]);
        mDevice_.destroyFence(mInFlightFences_[i]);
    }

    mDevice_.destroyCommandPool(mCommandPool_);
    mDevice_.destroy();

    if (enableValidationLayers) {
        DestroyDebugUtilsMessengerEXT(mInstance_, mDebugMessenger_, nullptr);
    }

    mInstance_.destroySurfaceKHR(mSurface_);
    mInstance_.destroy();
}

void GraphicsContextDesktop::createCommandBuffers() {
    mCommandBuffers_.resize(GraphicsContextDesktop::MAX_FRAMES_IN_FLIGHT);

    vk::CommandBufferAllocateInfo allocInfo{
        .commandPool = mCommandPool_,
        .level = vk::CommandBufferLevel::ePrimary,
        .commandBufferCount = (uint32_t)mCommandBuffers_.size(),
    };

    mCommandBuffers_ = mDevice_.allocateCommandBuffers(allocInfo);
}

vk::SampleCountFlagBits GraphicsContextDesktop::getMSAASamples() const {
    return mMSAASamples_;
}

void GraphicsContextDesktop::setVSync(bool enabled) {
    mVSyncEnabled_ = enabled;
    recreateSwapChain(mWindow_);
}


} // namespace clay

#endif // CLAY_PLATFORM_DESKTOP