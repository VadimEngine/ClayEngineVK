#ifdef CLAY_PLATFORM_ANDROID

// standard lib
#include <iostream>
#include <set>
#include <algorithm>
//class
#include "clay/graphics/android/GraphicsContextAndroid.h"
#include "clay/application/common/BaseScene.h"

namespace clay {

const bool enableValidationLayers = true;

// Requires VK_EXT_debug_utils instance extension
VKAPI_ATTR VkBool32 VKAPI_CALL debugMessengerCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData) {

    __android_log_print(ANDROID_LOG_ERROR, "Vulkan", "validation layer: %s", pCallbackData->pMessage);

    return VK_FALSE;
}

// fall back if VK_EXT_debug_utils is not available. Requires VK_EXT_debug_report
VKAPI_ATTR VkBool32 VKAPI_CALL debugReportCallback(
    VkDebugReportFlagsEXT flags,
    VkDebugReportObjectTypeEXT objectType,
    uint64_t object,
    size_t location,
    int32_t messageCode,
    const char* pLayerPrefix,
    const char* pMessage,
    void* pUserData) {

    __android_log_print(ANDROID_LOG_ERROR, "Vulkan", "validation layer: %s", pMessage);

    return VK_FALSE;
}

void GraphicsContextAndroid::setupDebugReportCallback() {
    vk::DebugReportCallbackCreateInfoEXT createInfo{};
    createInfo.flags = vk::DebugReportFlagBitsEXT::eError |
                       vk::DebugReportFlagBitsEXT::eWarning;
    createInfo.pfnCallback = debugReportCallback;
    createInfo.pUserData = nullptr;

    // Dynamic dispatch loader — loads extensions via vkGetInstanceProcAddr
    vk::DispatchLoaderDynamic dldi(mInstance_, vkGetInstanceProcAddr);

    // Create callback through dynamic loader
    mDebugReportCallback_ = mInstance_.createDebugReportCallbackEXT(createInfo, nullptr, dldi);
}

const std::vector<const char*> validationLayers = {
    "VK_LAYER_KHRONOS_validation"
};

const std::vector<const char*> deviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator) {
    auto func = (PFN_vkDestroyDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
    if (func != nullptr) {
        func(instance, debugMessenger, pAllocator);
    }
}

VkResult CreateDebugUtilsMessengerEXT(VkInstance instance,
                                      const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
                                      const VkAllocationCallbacks* pAllocator,
                                      VkDebugUtilsMessengerEXT* pDebugMessenger) {
    auto func = (PFN_vkCreateDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
    if (func != nullptr) {
        return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
    } else {
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
}

GraphicsContextAndroid::GraphicsContextAndroid(android_app* pAndroidApp) {
    initialize(pAndroidApp);
}

GraphicsContextAndroid::~GraphicsContextAndroid() {}

void GraphicsContextAndroid::initialize(android_app* pAndroidApp) {
    int32_t width = ANativeWindow_getWidth(pAndroidApp->window);
    int32_t height = ANativeWindow_getHeight(pAndroidApp->window);
    mFrameDimensions_ = {width, height};
    createInstance();
    // enumerate layers
    uint32_t layerCount;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
    std::vector<VkLayerProperties> layers(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, layers.data());

    for (const auto& layer : layers) {
        __android_log_print(ANDROID_LOG_INFO, "Vulkan", "Layer available: %s", layer.layerName);
    }
    //setupDebugMessenger();
//    VkAndroidSurfaceCreateInfoKHR createInfo{};
//    createInfo.sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR;
//    createInfo.window = pAndroidApp->window;
//
//    VkResult res = vkCreateAndroidSurfaceKHR((VkInstance)mInstance_, &createInfo, nullptr, &mSurface_);
//    if (res != VK_SUCCESS) {
//        throw std::runtime_error("vkCreateAndroidSurfaceKHR() failed");
//    }

    VkAndroidSurfaceCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR;
    createInfo.window = pAndroidApp->window;

    VkSurfaceKHR surface;
    VkResult res = vkCreateAndroidSurfaceKHR(
        static_cast<VkInstance>(mInstance_),
        &createInfo,
        nullptr,
        &surface
    );

    if (res != VK_SUCCESS) {
        throw std::runtime_error("vkCreateAndroidSurfaceKHR() failed");
    }

    mSurface_ = surface;

    pickPhysicalDevice();
    createLogicalDevice();
    createSwapChain(pAndroidApp);
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
}

bool GraphicsContextAndroid::checkValidationLayerSupport() {
    uint32_t layerCount;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

    std::vector<VkLayerProperties> availableLayers(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

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

bool GraphicsContextAndroid::checkDeviceExtensionSupport(vk::PhysicalDevice device) {
    uint32_t extensionCount;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

    std::vector<VkExtensionProperties> availableExtensions(extensionCount);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

    std::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());

    for (const auto& extension : availableExtensions) {
        std::string name(extension.extensionName); // stops at first '\0'
        requiredExtensions.erase(name);
    }

    return requiredExtensions.empty();
}

void GraphicsContextAndroid::setSurface(vk::SurfaceKHR surface) {
    mSurface_ = surface;
}

GraphicsContextAndroid::SwapChainSupportDetails GraphicsContextAndroid::querySwapChainSupport(vk::PhysicalDevice device) {
    SwapChainSupportDetails details;

    // Capabilities
    details.capabilities = device.getSurfaceCapabilitiesKHR(mSurface_);

    // Formats
    auto formats = device.getSurfaceFormatsKHR(mSurface_);
    details.formats = formats;

    // Present modes
    auto presentModes = device.getSurfacePresentModesKHR(mSurface_);
    details.presentModes = presentModes;

    return details;
}

void GraphicsContextAndroid::createLogicalDevice() {
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

    vk::PhysicalDeviceFeatures deviceFeatures{
        .samplerAnisotropy = VK_TRUE
    };

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

GraphicsContextAndroid::QueueFamilyIndices GraphicsContextAndroid::findQueueFamilies(vk::PhysicalDevice device) {
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

bool GraphicsContextAndroid::isDeviceSuitable(vk::PhysicalDevice device) {
    QueueFamilyIndices indices = findQueueFamilies(device);

    bool extensionsSupported = checkDeviceExtensionSupport(device);

    bool swapChainAdequate = false;
    if (extensionsSupported) {
        SwapChainSupportDetails swapChainSupport = querySwapChainSupport(device);
        swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
    }
    vk::PhysicalDeviceFeatures supportedFeatures = device.getFeatures();

    return indices.isComplete() && extensionsSupported && swapChainAdequate && supportedFeatures.samplerAnisotropy;
}

vk::SampleCountFlagBits GraphicsContextAndroid::getMaxUsableSampleCount() {
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

void GraphicsContextAndroid::pickPhysicalDevice() {
    std::vector<vk::PhysicalDevice> devices = mInstance_.enumeratePhysicalDevices();

    for (const auto& device : devices) {
        if (isDeviceSuitable(device)) {
            mPhysicalDevice_ = device;
            //mMSAASamples_ = getMaxUsableSampleCount();
            mMSAASamples_ = vk::SampleCountFlagBits::e1;
            break;
        }
    }
    // ERROR HERE
    if (!mPhysicalDevice_) {
        throw std::runtime_error("failed to find a suitable GPU!");
    }
}

std::vector<const char*> GraphicsContextAndroid::getRequiredExtensions() {
    std::vector<const char*> extensions = {
        VK_KHR_SURFACE_EXTENSION_NAME,
        "VK_KHR_surface",
        "VK_KHR_android_surface",
        "VK_KHR_swapchain"
    };

    if (enableValidationLayers) {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        extensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
    }

    return extensions;
}

void GraphicsContextAndroid::setupDebugMessenger() {
    if (!enableValidationLayers) return;

    vk::DebugUtilsMessengerCreateInfoEXT createInfo{};
    populateDebugMessengerCreateInfo(createInfo);

    // Load the function dynamically for Android
    vk::DispatchLoaderDynamic dldi(mInstance_, vkGetInstanceProcAddr);

    mDebugMessenger_ = mInstance_.createDebugUtilsMessengerEXT(createInfo, nullptr, dldi);
}

void GraphicsContextAndroid::populateDebugMessengerCreateInfo(vk::DebugUtilsMessengerCreateInfoEXT& createInfo) {
    createInfo = {
        .pNext = nullptr,
        .flags = 0,
        .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
        .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
        .pfnUserCallback = debugMessengerCallback,
        .pUserData = nullptr,
    };
}

void GraphicsContextAndroid::createInstance() {
    if (enableValidationLayers && !checkValidationLayerSupport()) {
        throw std::runtime_error("validation layers requested, but not available!");
    }

    // Application info
    vk::ApplicationInfo ai{
        .pApplicationName = "OpenXR Tutorial - Vulkan",
        .applicationVersion = 1,
        .pEngineName = "OpenXR Tutorial - Vulkan Engine",
        .engineVersion = 1,
        .apiVersion = VK_API_VERSION_1_0
    };

    // Check available extensions
    uint32_t instanceExtensionCount = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &instanceExtensionCount, nullptr);
    std::vector<VkExtensionProperties> instanceExtensionProperties(instanceExtensionCount);
    vkEnumerateInstanceExtensionProperties(nullptr, &instanceExtensionCount, instanceExtensionProperties.data());

    // Detect available extensions
    bool hasDebugUtils = false;
    bool hasDebugReport = false;
    for (const auto& ext : instanceExtensionProperties) {
        if (strcmp(ext.extensionName, VK_EXT_DEBUG_UTILS_EXTENSION_NAME) == 0) {
            hasDebugUtils = true;
        }
        if (strcmp(ext.extensionName, "VK_EXT_debug_report") == 0) {
            hasDebugReport = true;
        }
    }

    // Add required extensions
    activeInstanceExtensions.push_back("VK_KHR_surface");
    activeInstanceExtensions.push_back("VK_KHR_android_surface");
    if (hasDebugUtils) {
        activeInstanceExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    } else if (hasDebugReport) {
        activeInstanceExtensions.push_back("VK_EXT_debug_report");
    }

    if (enableValidationLayers) {
        activeInstanceLayers = {"VK_LAYER_KHRONOS_validation"};
    }

    vk::InstanceCreateInfo instanceCI{
        .pApplicationInfo = &ai,
        .enabledLayerCount = static_cast<uint32_t>(activeInstanceLayers.size()),
        .ppEnabledLayerNames = activeInstanceLayers.data(),
        .enabledExtensionCount = static_cast<uint32_t>(activeInstanceExtensions.size()),
        .ppEnabledExtensionNames = activeInstanceExtensions.data()
    };

    // Optional debug messenger for VK_EXT_debug_utils
    vk::DebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
    if (enableValidationLayers && hasDebugUtils) {
        populateDebugMessengerCreateInfo(debugCreateInfo);
        instanceCI.pNext = &debugCreateInfo;
    } else {
        instanceCI.pNext = nullptr;
    }

    // Create Vulkan instance
    vk::Result result = vk::createInstance(&instanceCI, nullptr, &mInstance_);
    if (result != vk::Result::eSuccess) {
        throw std::runtime_error("failed to create instance!");
    }

    // Setup appropriate debug mechanism
    if (enableValidationLayers) {
        if (hasDebugUtils) {
            setupDebugMessenger();
        } else if (hasDebugReport) {
            setupDebugReportCallback();
        }
    }
}

void GraphicsContextAndroid::createDescriptorPool() {
    uint32_t maxSets = 1024;
    std::vector<vk::DescriptorPoolSize> poolSizes{
        {vk::DescriptorType::eSampler,              16 * maxSets},
        {vk::DescriptorType::eSampledImage,         16 * maxSets},
        {vk::DescriptorType::eStorageImage,         16 * maxSets},
        {vk::DescriptorType::eUniformBuffer,        16 * maxSets},
        {vk::DescriptorType::eStorageBuffer,        16 * maxSets},
        {vk::DescriptorType::eCombinedImageSampler, 16 * maxSets},
    };

    vk::DescriptorPoolCreateInfo poolInfo {
        .pNext = nullptr,
        .flags =  vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
        .maxSets = maxSets,
        .poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
        .pPoolSizes = poolSizes.data()
    };

    mDescriptorPool_ = mDevice_.createDescriptorPool(poolInfo);
}

void GraphicsContextAndroid::generateMipmaps(vk::Image image, vk::Format imageFormat, int32_t texWidth, int32_t texHeight, uint32_t mipLevels) {
    // Check if image format supports linear blitting
    vk::FormatProperties formatProperties;
    formatProperties = mPhysicalDevice_.getFormatProperties(imageFormat);

    if (!(formatProperties.optimalTilingFeatures & vk::FormatFeatureFlagBits::eSampledImageFilterLinear)) {
        throw std::runtime_error("texture image format does not support linear blitting!");
    }

    vk::CommandBuffer commandBuffer = beginSingleTimeCommands();

    vk::ImageMemoryBarrier barrier{
        .sType = vk::StructureType::eImageMemoryBarrier,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = image,
        .subresourceRange = {
            .aspectMask = vk::ImageAspectFlagBits::eColor,
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

vk::Format GraphicsContextAndroid::findSupportedFormat(const std::vector<vk::Format>& candidates, vk::ImageTiling tiling, vk::FormatFeatureFlags features) {
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

vk::Format GraphicsContextAndroid::findDepthFormat() {
    return findSupportedFormat(
        {
            vk::Format::eD32Sfloat,
            vk::Format::eD32SfloatS8Uint,
            vk::Format::eD24UnormS8Uint
        },
        vk::ImageTiling::eOptimal,
        vk::FormatFeatureFlagBits::eDepthStencilAttachment
    );
}

void GraphicsContextAndroid::createRenderPass() {
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
        .stencilLoadOp = vk::AttachmentLoadOp::eDontCare,
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

void GraphicsContextAndroid::createCommandPool() {
    QueueFamilyIndices queueFamilyIndices = findQueueFamilies(mPhysicalDevice_);

    vk::CommandPoolCreateInfo poolInfo{
        .flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
        .queueFamilyIndex = queueFamilyIndices.graphicsFamily.value(),
    };

    mCommandPool_ = mDevice_.createCommandPool(poolInfo);
}

void GraphicsContextAndroid::createSwapChain(android_app* pAndroidApp) {
    SwapChainSupportDetails swapChainSupport = querySwapChainSupport(mPhysicalDevice_);

    vk::SurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
    vk::PresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
    vk::Extent2D extent = chooseSwapExtent(swapChainSupport.capabilities, pAndroidApp);

    uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
    if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount) {
        imageCount = swapChainSupport.capabilities.maxImageCount;
    }

    vk::CompositeAlphaFlagBitsKHR compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;
    vk::CompositeAlphaFlagsKHR supportedAlpha = swapChainSupport.capabilities.supportedCompositeAlpha;

    if (supportedAlpha & vk::CompositeAlphaFlagBitsKHR::eOpaque) {
        compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;
    } else if (supportedAlpha & vk::CompositeAlphaFlagBitsKHR::eInherit) {
        compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eInherit;
    } else if (supportedAlpha & vk::CompositeAlphaFlagBitsKHR::ePreMultiplied) {
        compositeAlpha = vk::CompositeAlphaFlagBitsKHR::ePreMultiplied;
    } else if (supportedAlpha & vk::CompositeAlphaFlagBitsKHR::ePostMultiplied) {
        compositeAlpha = vk::CompositeAlphaFlagBitsKHR::ePostMultiplied;
    } else {
        throw std::runtime_error("No supported composite alpha modes found.");
    }

    vk::SwapchainCreateInfoKHR createInfo{
        .surface = mSurface_,
        .minImageCount = imageCount,
        .imageFormat = surfaceFormat.format,
        .imageColorSpace = surfaceFormat.colorSpace,
        .imageExtent = extent,
        .imageArrayLayers = 1,
        .imageUsage = vk::ImageUsageFlagBits::eColorAttachment
    };

    QueueFamilyIndices indices = findQueueFamilies(mPhysicalDevice_);
    uint32_t queueFamilyIndices[] = {indices.graphicsFamily.value(), indices.presentFamily.value()};

    if (indices.graphicsFamily != indices.presentFamily) {
        createInfo.imageSharingMode = vk::SharingMode::eConcurrent;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = queueFamilyIndices;
    } else {
        createInfo.imageSharingMode = vk::SharingMode::eExclusive;
        createInfo.queueFamilyIndexCount = 0;
        createInfo.pQueueFamilyIndices = nullptr;
    }

    createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
    createInfo.compositeAlpha = compositeAlpha;
    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE;
    createInfo.oldSwapchain = nullptr;

    mSwapChain_ = mDevice_.createSwapchainKHR(createInfo);
    mSwapChainImages_ = mDevice_.getSwapchainImagesKHR(mSwapChain_);
    mSwapChainImageFormat_ = surfaceFormat.format;
    mSwapChainExtent_ = extent;
}

vk::SurfaceFormatKHR GraphicsContextAndroid::chooseSwapSurfaceFormat(const std::vector<vk::SurfaceFormatKHR>& availableFormats) {
    for (const auto& availableFormat : availableFormats) {
        if (availableFormat.format == vk::Format::eR8G8B8A8Srgb && availableFormat.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear) {
            return availableFormat;
        }
    }

    return availableFormats[0];
}

void GraphicsContextAndroid::createFramebuffers() {
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

vk::PresentModeKHR GraphicsContextAndroid::chooseSwapPresentMode(const std::vector<vk::PresentModeKHR>& availablePresentModes) {
    for (const auto& availablePresentMode : availablePresentModes) {
        if (availablePresentMode == vk::PresentModeKHR::eMailbox) {
            return availablePresentMode;
        }
    }

    return vk::PresentModeKHR::eFifo;
}

vk::Extent2D GraphicsContextAndroid::chooseSwapExtent(const vk::SurfaceCapabilitiesKHR& capabilities, android_app* pAndroidApp) {
    if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
        return capabilities.currentExtent;
    } else {
        //auto [width, height] = window.getDimensions();
        int32_t width = ANativeWindow_getWidth(pAndroidApp->window);
        int32_t height = ANativeWindow_getHeight(pAndroidApp->window);

        vk::Extent2D actualExtent = {
            static_cast<uint32_t>(width),
            static_cast<uint32_t>(height)
        };

        actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
        actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

        return actualExtent;
    }
}

void GraphicsContextAndroid::recreateSwapChain(android_app* pAndroidApp) {
    int32_t width = ANativeWindow_getWidth(pAndroidApp->window);
    int32_t height = ANativeWindow_getHeight(pAndroidApp->window);


//    while (width == 0 || height == 0) {
//        std::tie(width, height) = window.getDimensions();
//        glfwWaitEvents();
//    }

    mDevice_.waitIdle();

    cleanupSwapChain();

    createSwapChain(pAndroidApp);
    // createSwapChain();
    createImageViews();
    createColorResources();
    createDepthResources();
    createFramebuffers();
}

void GraphicsContextAndroid::cleanupSwapChain() {
    mDevice_.destroyImageView(mDepthImageView_, nullptr);
    mDevice_.destroyImage(mDepthImage_, nullptr);
    mDevice_.freeMemory(mDepthImageMemory_, nullptr);

    mDevice_.destroyImageView(mColorImageView_, nullptr);
    mDevice_.destroyImage(mColorImage_, nullptr);
    mDevice_.freeMemory(mColorImageMemory_, nullptr);

    for (const auto& framebuffer : mSwapChainFramebuffers_) {
        mDevice_.destroyFramebuffer(framebuffer, nullptr);
    }

    for (const auto& imageView : mSwapChainImageViews_) {
        mDevice_.destroyImageView(imageView, nullptr);
    }

    mDevice_.destroySwapchainKHR(mSwapChain_, nullptr);
}

void GraphicsContextAndroid::createImageViews() {
    mSwapChainImageViews_.resize(mSwapChainImages_.size());

    for (uint32_t i = 0; i < mSwapChainImages_.size(); i++) {
        mSwapChainImageViews_[i] = createImageView(mSwapChainImages_[i], mSwapChainImageFormat_, vk::ImageAspectFlagBits::eColor, 1);
    }
}

void GraphicsContextAndroid::createColorResources() {
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

void GraphicsContextAndroid::createDepthResources() {
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

    mDepthImageView_ = createImageView(mDepthImage_, depthFormat, vk::ImageAspectFlagBits::eDepth, 1);
}

void GraphicsContextAndroid::createSyncObjects() {
    mImageAvailableSemaphores_.resize(GraphicsContextAndroid::MAX_FRAMES_IN_FLIGHT);
    mRenderFinishedSemaphores_.resize(GraphicsContextAndroid::MAX_FRAMES_IN_FLIGHT);
    mInFlightFences_.resize(GraphicsContextAndroid::MAX_FRAMES_IN_FLIGHT);

    vk::SemaphoreCreateInfo semaphoreInfo{};

    vk::FenceCreateInfo fenceInfo{
        .flags = vk::FenceCreateFlagBits::eSignaled
    };

    for (size_t i = 0; i < GraphicsContextAndroid::MAX_FRAMES_IN_FLIGHT; i++) {
        try {
            mImageAvailableSemaphores_[i] = mDevice_.createSemaphore(semaphoreInfo);
            mRenderFinishedSemaphores_[i] = mDevice_.createSemaphore(semaphoreInfo);
            mInFlightFences_[i] = mDevice_.createFence(fenceInfo);
        } catch (const vk::SystemError& err) {
            throw std::runtime_error("failed to create synchronization objects for a frame!");
        }
    }
}

void GraphicsContextAndroid::cleanUp() {
    cleanupSwapChain();
    mCameraUniform_.reset();
    mDevice_.destroyRenderPass(mRenderPass_, nullptr);
    mDevice_.destroyDescriptorPool(mDescriptorPool_, nullptr);

    for (size_t i = 0; i < GraphicsContextAndroid::MAX_FRAMES_IN_FLIGHT; ++i) {
        mDevice_.destroySemaphore(mRenderFinishedSemaphores_[i], nullptr);
        mDevice_.destroySemaphore(mImageAvailableSemaphores_[i], nullptr);
        mDevice_.destroyFence(mInFlightFences_[i], nullptr);
    }

    mDevice_.destroyCommandPool(mCommandPool_, nullptr);
    mDevice_.destroy(nullptr);

    if (enableValidationLayers) {
        // mInstance_.destroyDebugUtilsMessengerEXT(mDebugMessenger_, nullptr);
    }
    mInstance_.destroySurfaceKHR(mSurface_, nullptr);
    mInstance_.destroy(nullptr);
}

void GraphicsContextAndroid::createCommandBuffers() {
    mCommandBuffers_.resize(GraphicsContextAndroid::MAX_FRAMES_IN_FLIGHT);

    vk::CommandBufferAllocateInfo allocInfo{
        .commandPool = mCommandPool_,
        .level = vk::CommandBufferLevel::ePrimary,
        .commandBufferCount = (uint32_t)mCommandBuffers_.size(),
    };

    mCommandBuffers_ = mDevice_.allocateCommandBuffers(allocInfo);
}

vk::SampleCountFlagBits GraphicsContextAndroid::getMSAASamples() const {
    return mMSAASamples_;
}

} // namespace clay

#endif // CLAY_PLATFORM_ANDROID