#ifdef CLAY_PLATFORM_DESKTOP

// standard lib
#include <iostream>
#include <set>
#include <algorithm>
// third party
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
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

// void DestroyDebugUtilsMessengerEXT(vk::Instance instance, vk::DebugUtilsMessengerEXT debugMessenger, const vk::AllocationCallbacks* pAllocator){
//     auto func = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
//         instance.getProcAddr("vkDestroyDebugUtilsMessengerEXT")
//     );

//     if (func) {
//         func(static_cast<VkInstance>(instance), static_cast<VkDebugUtilsMessengerEXT>(debugMessenger), reinterpret_cast<const VkAllocationCallbacks*>(pAllocator));
//     }
// }

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
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData) {

    std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;

    return vk::False;
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

    vk::PhysicalDeviceFeatures deviceFeatures{};
    deviceFeatures.samplerAnisotropy = vk::True;

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
    createInfo = {
        .messageSeverity = vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose | vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning | vk::DebugUtilsMessageSeverityFlagBitsEXT::eError,
        .messageType = vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral | vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation | vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance,
        .pfnUserCallback = debugCallback,
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

vk::Format GraphicsContextDesktop::findDepthFormat() {
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
        .imageUsage =  vk::ImageUsageFlagBits::eColorAttachment,
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
    
    mDepthImageView_ = createImageView(mDepthImage_, depthFormat, vk::ImageAspectFlagBits::eDepth, 1);
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
        // mInstance_.destroyDebugUtilsMessengerEXT(mDebugMessenger_, nullptr);
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