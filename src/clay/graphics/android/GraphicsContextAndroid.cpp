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
    VkDebugReportCallbackCreateInfoEXT createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
    createInfo.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT;
    createInfo.pfnCallback = debugReportCallback;
    createInfo.pUserData = nullptr;

    auto vkCreateDebugReportCallbackEXT =
        (PFN_vkCreateDebugReportCallbackEXT)vkGetInstanceProcAddr(mInstance_, "vkCreateDebugReportCallbackEXT");

    if (vkCreateDebugReportCallbackEXT != nullptr) {
        VkResult result = vkCreateDebugReportCallbackEXT(mInstance_, &createInfo, nullptr, &mDebugReportCallback_);
        if (result != VK_SUCCESS) {
            throw std::runtime_error("failed to set up debug report callback!");
        }
    } else {
        throw std::runtime_error("vkCreateDebugReportCallbackEXT not available.");
    }
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
    VkAndroidSurfaceCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR;
    createInfo.window = pAndroidApp->window;

    VkResult res = vkCreateAndroidSurfaceKHR(mInstance_, &createInfo, nullptr, &mSurface_);
    if (res != VK_SUCCESS) {
        throw std::runtime_error("vkCreateAndroidSurfaceKHR() failed");
    }
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

bool GraphicsContextAndroid::checkDeviceExtensionSupport(VkPhysicalDevice device) {
    uint32_t extensionCount;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

    std::vector<VkExtensionProperties> availableExtensions(extensionCount);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

    std::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());

    for (const auto& extension : availableExtensions) {
        requiredExtensions.erase(extension.extensionName);
    }

    return requiredExtensions.empty();
}

void GraphicsContextAndroid::setSurface(VkSurfaceKHR surface) {
    mSurface_ = surface;
}

GraphicsContextAndroid::SwapChainSupportDetails GraphicsContextAndroid::querySwapChainSupport(VkPhysicalDevice device) {
    SwapChainSupportDetails details;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, mSurface_, &details.capabilities);

    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, mSurface_, &formatCount, nullptr);

    if (formatCount != 0) {
        details.formats.resize(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, mSurface_, &formatCount, details.formats.data());
    }

    uint32_t presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, mSurface_, &presentModeCount, nullptr);

    if (presentModeCount != 0) {
        details.presentModes.resize(presentModeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, mSurface_, &presentModeCount, details.presentModes.data());
    }

    return details;
}

void GraphicsContextAndroid::createLogicalDevice() {
    QueueFamilyIndices indices = findQueueFamilies(mPhysicalDevice_);

    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    std::set<uint32_t> uniqueQueueFamilies = {indices.graphicsFamily.value(), indices.presentFamily.value()};

    float queuePriority = 1.0f;
    for (uint32_t queueFamily : uniqueQueueFamilies) {
        VkDeviceQueueCreateInfo queueCreateInfo{};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = queueFamily;
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;
        queueCreateInfos.push_back(queueCreateInfo);
    }

    VkPhysicalDeviceFeatures deviceFeatures{};
    deviceFeatures.samplerAnisotropy = VK_TRUE;

    VkDeviceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
    createInfo.pQueueCreateInfos = queueCreateInfos.data();
    createInfo.pEnabledFeatures = &deviceFeatures;
    createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
    createInfo.ppEnabledExtensionNames = deviceExtensions.data();

    if (enableValidationLayers) {
        createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
        createInfo.ppEnabledLayerNames = validationLayers.data();
    } else {
        createInfo.enabledLayerCount = 0;
    }

    if (vkCreateDevice(mPhysicalDevice_, &createInfo, nullptr, &mDevice_) != VK_SUCCESS) {
        throw std::runtime_error("failed to create logical device!");
    }

    vkGetDeviceQueue(mDevice_, indices.graphicsFamily.value(), 0, &mGraphicsQueue_);
    vkGetDeviceQueue(mDevice_, indices.presentFamily.value(), 0, &mPresentQueue_);
}

GraphicsContextAndroid::QueueFamilyIndices GraphicsContextAndroid::findQueueFamilies(VkPhysicalDevice device) {
    QueueFamilyIndices indices;

    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

    int i = 0;
    for (const auto& queueFamily : queueFamilies) {
        if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            indices.graphicsFamily = i;
        }

        VkBool32 presentSupport = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, mSurface_, &presentSupport);

        if (presentSupport) {
            indices.presentFamily = i;
        }

        if (indices.isComplete()) {
            break;
        }

        i++;
    }

    return indices;
}

bool GraphicsContextAndroid::isDeviceSuitable(VkPhysicalDevice device) {
    QueueFamilyIndices indices = findQueueFamilies(device);

    bool extensionsSupported = checkDeviceExtensionSupport(device);

    bool swapChainAdequate = false;
    if (extensionsSupported) {
        SwapChainSupportDetails swapChainSupport = querySwapChainSupport(device);
        swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
    }

    VkPhysicalDeviceFeatures supportedFeatures;
    vkGetPhysicalDeviceFeatures(device, &supportedFeatures);

    return indices.isComplete() && extensionsSupported && swapChainAdequate && supportedFeatures.samplerAnisotropy;
}

VkSampleCountFlagBits GraphicsContextAndroid::getMaxUsableSampleCount() {
    VkPhysicalDeviceProperties physicalDeviceProperties;
    vkGetPhysicalDeviceProperties(mPhysicalDevice_, &physicalDeviceProperties);

    VkSampleCountFlags counts = physicalDeviceProperties.limits.framebufferColorSampleCounts & physicalDeviceProperties.limits.framebufferDepthSampleCounts;
    if (counts & VK_SAMPLE_COUNT_64_BIT) { return VK_SAMPLE_COUNT_64_BIT; }
    if (counts & VK_SAMPLE_COUNT_32_BIT) { return VK_SAMPLE_COUNT_32_BIT; }
    if (counts & VK_SAMPLE_COUNT_16_BIT) { return VK_SAMPLE_COUNT_16_BIT; }
    if (counts & VK_SAMPLE_COUNT_8_BIT) { return VK_SAMPLE_COUNT_8_BIT; }
    if (counts & VK_SAMPLE_COUNT_4_BIT) { return VK_SAMPLE_COUNT_4_BIT; }
    if (counts & VK_SAMPLE_COUNT_2_BIT) { return VK_SAMPLE_COUNT_2_BIT; }

    return VK_SAMPLE_COUNT_1_BIT;
}

void GraphicsContextAndroid::pickPhysicalDevice() {
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(mInstance_, &deviceCount, nullptr);

    if (deviceCount == 0) {
        throw std::runtime_error("failed to find GPUs with Vulkan support!");
    }

    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(mInstance_, &deviceCount, devices.data());

    for (const auto& device : devices) {
        if (isDeviceSuitable(device)) {
            mPhysicalDevice_ = device;
            //mMSAASamples_ = getMaxUsableSampleCount();
            mMSAASamples_ = VK_SAMPLE_COUNT_1_BIT;
            break;
        }
    }

    if (mPhysicalDevice_ == VK_NULL_HANDLE) {
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

    VkDebugUtilsMessengerCreateInfoEXT createInfo{};
    populateDebugMessengerCreateInfo(createInfo);

    if (CreateDebugUtilsMessengerEXT(mInstance_, &createInfo, nullptr, &mDebugMessenger_) != VK_SUCCESS) {
        throw std::runtime_error("failed to set up debug messenger!");
    }
}

void GraphicsContextAndroid::populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo) {
    createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    createInfo.pfnUserCallback = debugMessengerCallback;
    createInfo.pNext = nullptr;
    createInfo.flags = 0;
    createInfo.pUserData = nullptr;
}

void GraphicsContextAndroid::createInstance() {
    if (enableValidationLayers && !checkValidationLayerSupport()) {
        throw std::runtime_error("validation layers requested, but not available!");
    }

    // Application info
    VkApplicationInfo ai{};
    ai.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    ai.pApplicationName = "OpenXR Tutorial - Vulkan";
    ai.applicationVersion = 1;
    ai.pEngineName = "OpenXR Tutorial - Vulkan Engine";
    ai.engineVersion = 1;
    ai.apiVersion = VK_API_VERSION_1_0;

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

    VkInstanceCreateInfo instanceCI{};
    instanceCI.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instanceCI.pApplicationInfo = &ai;
    instanceCI.enabledLayerCount = static_cast<uint32_t>(activeInstanceLayers.size());
    instanceCI.ppEnabledLayerNames = activeInstanceLayers.data();
    instanceCI.enabledExtensionCount = static_cast<uint32_t>(activeInstanceExtensions.size());
    instanceCI.ppEnabledExtensionNames = activeInstanceExtensions.data();

    // Optional debug messenger for VK_EXT_debug_utils
    VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
    if (enableValidationLayers && hasDebugUtils) {
        populateDebugMessengerCreateInfo(debugCreateInfo);
        instanceCI.pNext = &debugCreateInfo;
    } else {
        instanceCI.pNext = nullptr;
    }

    // Create Vulkan instance
    VkResult result = vkCreateInstance(&instanceCI, nullptr, &mInstance_);
    if (result != VK_SUCCESS) {
        throw std::runtime_error("failed to create Vulkan instance");
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
    std::vector<VkDescriptorPoolSize> poolSizes{
        {VK_DESCRIPTOR_TYPE_SAMPLER, 16 * maxSets},
        {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 16 * maxSets},
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 16 * maxSets},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 16 * maxSets},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 16 * maxSets},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 16 * maxSets},
    };

    VkDescriptorPoolCreateInfo poolInfo;
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.pNext = nullptr;
    poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    poolInfo.maxSets = maxSets;
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();

    if (vkCreateDescriptorPool(mDevice_, &poolInfo, nullptr, &mDescriptorPool_) != VK_SUCCESS) {
        throw std::runtime_error("failed to create descriptor pool!");
    }
}

void GraphicsContextAndroid::generateMipmaps(VkImage image, VkFormat imageFormat, int32_t texWidth, int32_t texHeight, uint32_t mipLevels) {
    // Check if image format supports linear blitting
    VkFormatProperties formatProperties;
    vkGetPhysicalDeviceFormatProperties(mPhysicalDevice_, imageFormat, &formatProperties);

    if (!(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT)) {
        throw std::runtime_error("texture image format does not support linear blitting!");
    }

    VkCommandBuffer commandBuffer = beginSingleTimeCommands();

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.image = image;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.subresourceRange.levelCount = 1;

    int32_t mipWidth = texWidth;
    int32_t mipHeight = texHeight;

    for (uint32_t i = 1; i < mipLevels; i++) {
        barrier.subresourceRange.baseMipLevel = i - 1;
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

        vkCmdPipelineBarrier(commandBuffer,
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
            0, nullptr,
            0, nullptr,
            1, &barrier);

        VkImageBlit blit{};
        blit.srcOffsets[0] = {0, 0, 0};
        blit.srcOffsets[1] = {mipWidth, mipHeight, 1};
        blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        blit.srcSubresource.mipLevel = i - 1;
        blit.srcSubresource.baseArrayLayer = 0;
        blit.srcSubresource.layerCount = 1;
        blit.dstOffsets[0] = {0, 0, 0};
        blit.dstOffsets[1] = { mipWidth > 1 ? mipWidth / 2 : 1, mipHeight > 1 ? mipHeight / 2 : 1, 1 };
        blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        blit.dstSubresource.mipLevel = i;
        blit.dstSubresource.baseArrayLayer = 0;
        blit.dstSubresource.layerCount = 1;

        vkCmdBlitImage(commandBuffer,
            image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1, &blit,
            VK_FILTER_LINEAR);

        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        vkCmdPipelineBarrier(commandBuffer,
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
            0, nullptr,
            0, nullptr,
            1, &barrier);

        if (mipWidth > 1) mipWidth /= 2;
        if (mipHeight > 1) mipHeight /= 2;
    }

    barrier.subresourceRange.baseMipLevel = mipLevels - 1;
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(commandBuffer,
        VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
        0, nullptr,
        0, nullptr,
        1, &barrier);

    endSingleTimeCommands(commandBuffer);
}

VkFormat GraphicsContextAndroid::findSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features) {
    for (VkFormat format : candidates) {
        VkFormatProperties props;
        vkGetPhysicalDeviceFormatProperties(mPhysicalDevice_, format, &props);

        if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features) {
            return format;
        } else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features) {
            return format;
        }
    }

    throw std::runtime_error("failed to find supported format!");
}

VkFormat GraphicsContextAndroid::findDepthFormat() {
    return findSupportedFormat(
    {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT},
        VK_IMAGE_TILING_OPTIMAL,
        VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
    );
}

void GraphicsContextAndroid::createRenderPass() {
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = mSwapChainImageFormat_;
    colorAttachment.samples = mMSAASamples_;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = (mMSAASamples_ == VK_SAMPLE_COUNT_1_BIT) ? VK_IMAGE_LAYOUT_PRESENT_SRC_KHR : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentDescription depthAttachment{};
    depthAttachment.format = findDepthFormat();
    depthAttachment.samples = mMSAASamples_;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentDescription colorAttachmentResolve{};
    colorAttachmentResolve.format = mSwapChainImageFormat_;
    colorAttachmentResolve.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachmentResolve.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachmentResolve.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachmentResolve.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachmentResolve.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachmentResolve.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachmentResolve.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depthAttachmentRef{};
    depthAttachmentRef.attachment = 1;
    depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference colorAttachmentResolveRef{};
    colorAttachmentResolveRef.attachment = 2;
    colorAttachmentResolveRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;
    subpass.pDepthStencilAttachment = &depthAttachmentRef;
    if (mMSAASamples_ > VK_SAMPLE_COUNT_1_BIT) {
        subpass.pResolveAttachments = &colorAttachmentResolveRef;
    }

    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    std::vector<VkAttachmentDescription> attachments;
    if (mMSAASamples_ > VK_SAMPLE_COUNT_1_BIT) {
        attachments = { colorAttachment, depthAttachment, colorAttachmentResolve };
    } else {
        attachments = { colorAttachment, depthAttachment };
    }
    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    renderPassInfo.pAttachments = attachments.data();
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;

    if (vkCreateRenderPass(mDevice_, &renderPassInfo, nullptr, &mRenderPass_) != VK_SUCCESS) {
        throw std::runtime_error("failed to create render pass!");
    }
}

void GraphicsContextAndroid::createCommandPool() {
    QueueFamilyIndices queueFamilyIndices = findQueueFamilies(mPhysicalDevice_);

    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();

    if (vkCreateCommandPool(mDevice_, &poolInfo, nullptr, &mCommandPool_) != VK_SUCCESS) {
        throw std::runtime_error("failed to create command pool!");
    }
}

void GraphicsContextAndroid::createSwapChain(android_app* pAndroidApp) {
    SwapChainSupportDetails swapChainSupport = querySwapChainSupport(mPhysicalDevice_);

    VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
    VkPresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
    VkExtent2D extent = chooseSwapExtent(swapChainSupport.capabilities, pAndroidApp);

    uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
    if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount) {
        imageCount = swapChainSupport.capabilities.maxImageCount;
    }

    // FIX: Select a supported compositeAlpha mode
    VkCompositeAlphaFlagBitsKHR compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    VkCompositeAlphaFlagsKHR supportedAlpha = swapChainSupport.capabilities.supportedCompositeAlpha;

    if (supportedAlpha & VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR) {
        compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    } else if (supportedAlpha & VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR) {
        compositeAlpha = VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR;
    } else if (supportedAlpha & VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR) {
        compositeAlpha = VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR;
    } else if (supportedAlpha & VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR) {
        compositeAlpha = VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR;
    } else {
        throw std::runtime_error("No supported composite alpha modes found.");
    }

    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = mSurface_;
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    QueueFamilyIndices indices = findQueueFamilies(mPhysicalDevice_);
    uint32_t queueFamilyIndices[] = {indices.graphicsFamily.value(), indices.presentFamily.value()};

    if (indices.graphicsFamily != indices.presentFamily) {
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = queueFamilyIndices;
    } else {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        createInfo.queueFamilyIndexCount = 0;
        createInfo.pQueueFamilyIndices = nullptr;
    }

    createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
    createInfo.compositeAlpha = compositeAlpha; // <-- FIXED HERE
    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE;
    createInfo.oldSwapchain = VK_NULL_HANDLE;

    if (vkCreateSwapchainKHR(mDevice_, &createInfo, nullptr, &mSwapChain_) != VK_SUCCESS) {
        throw std::runtime_error("failed to create swap chain!");
    }

    vkGetSwapchainImagesKHR(mDevice_, mSwapChain_, &imageCount, nullptr);
    mSwapChainImages_.resize(imageCount);
    vkGetSwapchainImagesKHR(mDevice_, mSwapChain_, &imageCount, mSwapChainImages_.data());

    mSwapChainImageFormat_ = surfaceFormat.format;
    mSwapChainExtent_ = extent;
}

VkSurfaceFormatKHR GraphicsContextAndroid::chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) {
    for (const auto& availableFormat : availableFormats) {
        if (availableFormat.format == VK_FORMAT_B8G8R8A8_UNORM && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return availableFormat;
        }
    }

    return availableFormats[0];
}

void GraphicsContextAndroid::createFramebuffers() {
    mSwapChainFramebuffers_.resize(mSwapChainImageViews_.size());

    for (size_t i = 0; i < mSwapChainImageViews_.size(); i++) {
        std::vector<VkImageView> attachments;

        if (mMSAASamples_ > VK_SAMPLE_COUNT_1_BIT) {
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

        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = mRenderPass_;
        framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        framebufferInfo.pAttachments = attachments.data();
        framebufferInfo.width = mSwapChainExtent_.width;
        framebufferInfo.height = mSwapChainExtent_.height;
        framebufferInfo.layers = 1;

        if (vkCreateFramebuffer(mDevice_, &framebufferInfo, nullptr, &mSwapChainFramebuffers_[i]) != VK_SUCCESS) {
            throw std::runtime_error("failed to create framebuffer!");
        }
    }
}

VkPresentModeKHR GraphicsContextAndroid::chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes) {
    for (const auto& availablePresentMode : availablePresentModes) {
        if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
            return availablePresentMode;
        }
    }

    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D GraphicsContextAndroid::chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities, android_app* pAndroidApp) {
    if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
        return capabilities.currentExtent;
    } else {
        //auto [width, height] = window.getDimensions();
        int32_t width = ANativeWindow_getWidth(pAndroidApp->window);
        int32_t height = ANativeWindow_getHeight(pAndroidApp->window);

        VkExtent2D actualExtent = {
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

    vkDeviceWaitIdle(mDevice_);

    cleanupSwapChain();

    createSwapChain(pAndroidApp);
    // createSwapChain();
    createImageViews();
    createColorResources();
    createDepthResources();
    createFramebuffers();
}

void GraphicsContextAndroid::cleanupSwapChain() {
    vkDestroyImageView(mDevice_, mDepthImageView_, nullptr);
    vkDestroyImage(mDevice_, mDepthImage_, nullptr);
    vkFreeMemory(mDevice_, mDepthImageMemory_, nullptr);

    vkDestroyImageView(mDevice_, mColorImageView_, nullptr);
    vkDestroyImage(mDevice_, mColorImage_, nullptr);
    vkFreeMemory(mDevice_, mColorImageMemory_, nullptr);

    for (auto framebuffer : mSwapChainFramebuffers_) {
        vkDestroyFramebuffer(mDevice_, framebuffer, nullptr);
    }

    for (auto imageView : mSwapChainImageViews_) {
        vkDestroyImageView(mDevice_, imageView, nullptr);
    }

    vkDestroySwapchainKHR(mDevice_, mSwapChain_, nullptr);
}

void GraphicsContextAndroid::createImageViews() {
    mSwapChainImageViews_.resize(mSwapChainImages_.size());

    for (uint32_t i = 0; i < mSwapChainImages_.size(); i++) {
        mSwapChainImageViews_[i] = createImageView(mSwapChainImages_[i], mSwapChainImageFormat_, VK_IMAGE_ASPECT_COLOR_BIT, 1);
    }
}

void GraphicsContextAndroid::createColorResources() {
    VkFormat colorFormat = mSwapChainImageFormat_;

    createImage(
        mSwapChainExtent_.width,
        mSwapChainExtent_.height,
        1,
        mMSAASamples_,
        colorFormat,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        mColorImage_,
        mColorImageMemory_
    );
    mColorImageView_ = createImageView(mColorImage_, colorFormat, VK_IMAGE_ASPECT_COLOR_BIT, 1);
}

void GraphicsContextAndroid::createDepthResources() {
    VkFormat depthFormat = findDepthFormat();

    createImage(
        mSwapChainExtent_.width,
        mSwapChainExtent_.height,
        1,
        mMSAASamples_,
        depthFormat,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        mDepthImage_,
        mDepthImageMemory_
    );
    mDepthImageView_ = createImageView(mDepthImage_, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT, 1);
}

void GraphicsContextAndroid::createSyncObjects() {
    mImageAvailableSemaphores_.resize(GraphicsContextAndroid::MAX_FRAMES_IN_FLIGHT);
    mRenderFinishedSemaphores_.resize(GraphicsContextAndroid::MAX_FRAMES_IN_FLIGHT);
    mInFlightFences_.resize(GraphicsContextAndroid::MAX_FRAMES_IN_FLIGHT);

    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (size_t i = 0; i < GraphicsContextAndroid::MAX_FRAMES_IN_FLIGHT; i++) {
        if (vkCreateSemaphore(mDevice_, &semaphoreInfo, nullptr, &mImageAvailableSemaphores_[i]) != VK_SUCCESS ||
            vkCreateSemaphore(mDevice_, &semaphoreInfo, nullptr, &mRenderFinishedSemaphores_[i]) != VK_SUCCESS ||
            vkCreateFence(mDevice_, &fenceInfo, nullptr, &mInFlightFences_[i]) != VK_SUCCESS) {
            throw std::runtime_error("failed to create synchronization objects for a frame!");
        }
    }
}

void GraphicsContextAndroid::cleanUp() {
    cleanupSwapChain();
    mCameraUniform_.reset();
    vkDestroyRenderPass(mDevice_, mRenderPass_, nullptr);

    vkDestroyDescriptorPool(mDevice_, mDescriptorPool_, nullptr);

    for (size_t i = 0; i < GraphicsContextAndroid::MAX_FRAMES_IN_FLIGHT; i++) {
        vkDestroySemaphore(mDevice_, mRenderFinishedSemaphores_[i], nullptr);
        vkDestroySemaphore(mDevice_, mImageAvailableSemaphores_[i], nullptr);
        vkDestroyFence(mDevice_, mInFlightFences_[i], nullptr);
    }

    vkDestroyCommandPool(mDevice_, mCommandPool_, nullptr);

    vkDestroyDevice(mDevice_, nullptr);

    if (enableValidationLayers) {
        DestroyDebugUtilsMessengerEXT(mInstance_, mDebugMessenger_, nullptr);
    }

    vkDestroySurfaceKHR(mInstance_, mSurface_, nullptr);
    vkDestroyInstance(mInstance_, nullptr);
}

void GraphicsContextAndroid::createCommandBuffers() {
    mCommandBuffers_.resize(GraphicsContextAndroid::MAX_FRAMES_IN_FLIGHT);

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = mCommandPool_;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = (uint32_t)mCommandBuffers_.size();

    if (vkAllocateCommandBuffers(mDevice_, &allocInfo, mCommandBuffers_.data()) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate command buffers!");
    }
}

VkSampleCountFlagBits GraphicsContextAndroid::getMSAASamples() const {
    return mMSAASamples_;
}

} // namespace clay

#endif // CLAY_PLATFORM_ANDROID