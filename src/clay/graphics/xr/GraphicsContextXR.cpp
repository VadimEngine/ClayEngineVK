#ifdef CLAY_PLATFORM_XR

// third party
#include <glm/glm.hpp>
// clay
#include "clay/utils/common/Logger.h"
// class
#include "clay/graphics/xr/GraphicsContextXR.h"


#define VK_MAKE_API_VERSION(variant, major, minor, patch) VK_MAKE_VERSION(major, minor, patch)

namespace clay {

// disable validation when using RenderDoc
bool enableValidation = true;

const std::vector<const char*> validationLayers = {
    "VK_LAYER_KHRONOS_validation"
};

VKAPI_ATTR VkBool32 VKAPI_CALL GraphicsContextXR::debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData) {

    std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;

    return VK_FALSE;
}

int64_t GraphicsContextXR::SelectColorSwapchainFormat(const std::vector<int64_t> &formats) {
    const std::vector<int64_t> &supportSwapchainFormats = GetSupportedColorSwapchainFormats();

    const auto swapchainFormatIt = std::find_first_of(
        formats.begin(),
        formats.end(),
        std::begin(supportSwapchainFormats),
        std::end(supportSwapchainFormats)
    );
    if (swapchainFormatIt == formats.end()) {
        std::cout << "ERROR: Unable to find supported Color Swapchain Format" << std::endl;
        DEBUG_BREAK;
        return 0;
    }

    return *swapchainFormatIt;
}

int64_t GraphicsContextXR::SelectDepthSwapchainFormat(const std::vector<int64_t>& formats) {
    const std::vector<int64_t> &supportSwapchainFormats = GetSupportedDepthSwapchainFormats();

    const auto swapchainFormatIt = std::find_first_of(
        formats.begin(),
        formats.end(),
        std::begin(supportSwapchainFormats),
        std::end(supportSwapchainFormats)
    );
    if (swapchainFormatIt == formats.end()) {
        std::cout << "ERROR: Unable to find supported Depth Swapchain Format" << std::endl;
        DEBUG_BREAK;
        return 0;
    }

    return *swapchainFormatIt;
}

static bool MemoryTypeFromProperties(VkPhysicalDeviceMemoryProperties memoryProperties,
                                     uint32_t typeBits,
                                     VkMemoryPropertyFlags requirementsMask,
                                     uint32_t *typeIndex) {
    // Search memory types to find first index with those properties
    for (uint32_t i = 0; i < VK_MAX_MEMORY_TYPES; i++) {
        if ((typeBits & 1) == 1) {
            // Type is available, does it match user properties?
            if ((memoryProperties.memoryTypes[i].propertyFlags & requirementsMask) == requirementsMask) {
                *typeIndex = i;
                return true;
            }
        }
        typeBits >>= 1;
    }
    // No memory types matched, return failure
    return false;
}

vk::DescriptorType ToVkDescriptorType2(const GraphicsContextXR::DescriptorInfo &descInfo) {
    vk::DescriptorType vkType;
    switch (descInfo.type) {
        default:
        case GraphicsContextXR::DescriptorInfo::Type::BUFFER: {
            vkType = descInfo.readWrite ? vk::DescriptorType::eStorageBuffer : vk::DescriptorType::eUniformBuffer;
            break;
        }
        case GraphicsContextXR::DescriptorInfo::Type::IMAGE: {
            vkType = descInfo.readWrite ? vk::DescriptorType::eStorageImage : vk::DescriptorType::eSampledImage;
            break;
        }
        case GraphicsContextXR::DescriptorInfo::Type::SAMPLER: {
            vkType = vk::DescriptorType::eSampler;
            break;
        }
    }
    return vkType;
}

void GraphicsContextXR::populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo) {
    createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    createInfo.pfnUserCallback = debugCallback;
}

bool GraphicsContextXR::checkValidationLayerSupport() {
    uint32_t layerCount;
    // If this return 0, then it means app\src\main\jniLibs is missing validation .so
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
    std::cout << layerCount << " extensions supported" << std::endl;

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

// TODO remove this
GraphicsContextXR::GraphicsContextXR() {
    // Instance
    vk::ApplicationInfo ai;
    ai.pNext = nullptr;
    ai.pApplicationName = "OpenXR Tutorial - Vulkan";
    ai.applicationVersion = 1;
    ai.pEngineName = "OpenXR Tutorial - Vulkan Engine";
    ai.engineVersion = 1;
    ai.apiVersion = VK_MAKE_API_VERSION(0, 1, 0, 0);

    uint32_t instanceExtensionCount = 0;
    vkEnumerateInstanceExtensionProperties(
        nullptr,
        &instanceExtensionCount,
        nullptr
    );

    std::vector<VkExtensionProperties> instanceExtensionProperties;
    instanceExtensionProperties.resize(instanceExtensionCount);
    vkEnumerateInstanceExtensionProperties(
        nullptr,
        &instanceExtensionCount,
        instanceExtensionProperties.data()
    );
    const std::vector<std::string> &instanceExtensionNames = {VK_KHR_SURFACE_EXTENSION_NAME, VK_KHR_ANDROID_SURFACE_EXTENSION_NAME};
    for (const std::string &requestExtension : instanceExtensionNames) {
        for (const VkExtensionProperties &extensionProperty : instanceExtensionProperties) {
            if (strcmp(requestExtension.c_str(), extensionProperty.extensionName) != 0) {
                continue;
            } else {
                activeInstanceExtensions.push_back(requestExtension.c_str());
            }
            break;
        }
    }

    if (enableValidation) {
        activeInstanceLayers = {"VK_LAYER_KHRONOS_validation"};
    }

    vk::InstanceCreateInfo instanceCI;
    instanceCI.pNext = nullptr;
    instanceCI.flags = {};
    instanceCI.pApplicationInfo = &ai;
    instanceCI.enabledLayerCount = static_cast<uint32_t>(activeInstanceLayers.size());
    instanceCI.ppEnabledLayerNames = activeInstanceLayers.data();
    instanceCI.enabledExtensionCount = static_cast<uint32_t>(activeInstanceExtensions.size());
    instanceCI.ppEnabledExtensionNames = activeInstanceExtensions.data();
    vk::createInstance(&instanceCI, nullptr, &mInstance_);

    // Physical Device
    std::vector<vk::PhysicalDevice> physicalDevices = mInstance_.enumeratePhysicalDevices();
    // Select the first available device.
    mPhysicalDevice_ = physicalDevices[0];

    // Device
    std::vector<VkQueueFamilyProperties> queueFamilyProperties;
    uint32_t queueFamilyPropertiesCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(
        mPhysicalDevice_,
        &queueFamilyPropertiesCount,
        nullptr
    );
    queueFamilyProperties.resize(queueFamilyPropertiesCount);
    vkGetPhysicalDeviceQueueFamilyProperties(
        mPhysicalDevice_,
        &queueFamilyPropertiesCount,
        queueFamilyProperties.data()
    );

    std::vector<vk::DeviceQueueCreateInfo> deviceQueueCIs;
    std::vector<std::vector<float>> queuePriorities;
    queuePriorities.resize(queueFamilyProperties.size());
    deviceQueueCIs.resize(queueFamilyProperties.size());
    for (size_t i = 0; i < deviceQueueCIs.size(); i++) {
        for (size_t j = 0; j < queueFamilyProperties[i].queueCount; j++) {
            queuePriorities[i].push_back(1.0f);
        }

        deviceQueueCIs[i].pNext = nullptr;
        deviceQueueCIs[i].flags = {};
        deviceQueueCIs[i].queueFamilyIndex = static_cast<uint32_t>(i);
        deviceQueueCIs[i].queueCount = queueFamilyProperties[i].queueCount;
        deviceQueueCIs[i].pQueuePriorities = queuePriorities[i].data();

        if (BitwiseCheck(queueFamilyProperties[i].queueFlags, VkQueueFlags(VK_QUEUE_GRAPHICS_BIT)) &&
            queueFamilyIndex == 0xFFFFFFFF && queueIndex == 0xFFFFFFFF) {
            queueFamilyIndex = static_cast<uint32_t>(i);
            queueIndex = 0;
        }
    }

    uint32_t deviceExtensionCount = 0;
    vkEnumerateDeviceExtensionProperties(
        mPhysicalDevice_,
        nullptr,
        &deviceExtensionCount,
        nullptr
    );
    std::vector<VkExtensionProperties> deviceExtensionProperties;
    deviceExtensionProperties.resize(deviceExtensionCount);

    vkEnumerateDeviceExtensionProperties(
        mPhysicalDevice_,
        nullptr,
        &deviceExtensionCount,
        deviceExtensionProperties.data()
    );

    const std::vector<std::string> &deviceExtensionNames = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
    for (const std::string &requestExtension : deviceExtensionNames) {
        for (const VkExtensionProperties &extensionProperty : deviceExtensionProperties) {
            if (strcmp(requestExtension.c_str(), extensionProperty.extensionName) != 0) {
                continue;
            } else {
                activeDeviceExtensions.push_back(requestExtension.c_str());
            }
            break;
        }
    }

    vk::PhysicalDeviceFeatures features = mPhysicalDevice_.getFeatures();

    vk::DeviceCreateInfo deviceCI{};
    deviceCI.pNext = nullptr;
    deviceCI.flags = {};
    deviceCI.queueCreateInfoCount = static_cast<uint32_t>(deviceQueueCIs.size());
    deviceCI.pQueueCreateInfos = deviceQueueCIs.data();
    deviceCI.enabledExtensionCount = static_cast<uint32_t>(activeDeviceExtensions.size());
    deviceCI.ppEnabledExtensionNames = activeDeviceExtensions.data();
    deviceCI.pEnabledFeatures = &features;
    if (enableValidation) {
        deviceCI.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
        deviceCI.ppEnabledLayerNames = validationLayers.data();
    } else {
        deviceCI.enabledLayerCount = 0;
        deviceCI.ppEnabledLayerNames = nullptr;
    }
    mDevice_ = mPhysicalDevice_.createDevice(deviceCI);


    vk::CommandPoolCreateInfo cmdPoolCI;
    cmdPoolCI.pNext = nullptr;
    cmdPoolCI.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer;
    cmdPoolCI.queueFamilyIndex = queueFamilyIndex;

    mCommandPool_ = mDevice_.createCommandPool(cmdPoolCI);


    vk::CommandBufferAllocateInfo allocateInfo;
    allocateInfo.pNext = nullptr;
    allocateInfo.commandPool = mCommandPool_;
    allocateInfo.level = vk::CommandBufferLevel::ePrimary;
    allocateInfo.commandBufferCount = 1;

    cmdBuffer = mDevice_.allocateCommandBuffers(allocateInfo)[0];

    mGraphicsQueue_ = mDevice_.getQueue(queueFamilyIndex, queueIndex);


    vk::FenceCreateInfo fenceCI{};
    fenceCI.pNext = nullptr;
    fenceCI.flags = vk::FenceCreateFlagBits::eSignaled;

    fence = mDevice_.createFence(fenceCI);

    uint32_t maxSets = 1024;
    std::vector<vk::DescriptorPoolSize> poolSizes{
        {vk::DescriptorType::eSampler, 16 * maxSets},
        {vk::DescriptorType::eSampledImage, 16 * maxSets},
        {vk::DescriptorType::eStorageImage, 16 * maxSets},
        {vk::DescriptorType::eUniformBuffer, 16 * maxSets},
        {vk::DescriptorType::eStorageBuffer, 16 * maxSets},
        {vk::DescriptorType::eCombinedImageSampler, 16 * maxSets}
    };

    vk::DescriptorPoolCreateInfo descPoolCI;
    descPoolCI.pNext = nullptr;
    descPoolCI.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet;
    descPoolCI.maxSets = maxSets;
    descPoolCI.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    descPoolCI.pPoolSizes = poolSizes.data();

    mDescriptorPool_ = mDevice_.createDescriptorPool(descPoolCI);
}

// im using this constructor
GraphicsContextXR::GraphicsContextXR(XrInstance m_xrInstance, XrSystemId systemId) {
    if (enableValidation && !checkValidationLayerSupport()) {
        throw std::runtime_error("validation layers requested, but not available!");
    }
    // Instance
    LoadPFN_XrFunctions(m_xrInstance);

    XrGraphicsRequirementsVulkanKHR graphicsRequirements{XR_TYPE_GRAPHICS_REQUIREMENTS_VULKAN_KHR};
    OPENXR_CHECK(
        xrGetVulkanGraphicsRequirementsKHR(m_xrInstance, systemId, &graphicsRequirements),
        "Failed to get Graphics Requirements for Vulkan."
    )

    vk::ApplicationInfo ai;
    ai.pNext = nullptr;
    ai.pApplicationName = "OpenXR Tutorial - Vulkan";
    ai.applicationVersion = 1;
    ai.pEngineName = "OpenXR Tutorial - Vulkan Engine";
    ai.engineVersion = 1;
    ai.apiVersion = VK_MAKE_API_VERSION(
        0,
        XR_VERSION_MAJOR(graphicsRequirements.minApiVersionSupported),
        XR_VERSION_MINOR(graphicsRequirements.minApiVersionSupported),
        0
    );

    uint32_t instanceExtensionCount = 0;

    std::vector<vk::ExtensionProperties> instanceExtensionProperties = vk::enumerateInstanceExtensionProperties();
    const std::vector<std::string> &openXrInstanceExtensionNames = GetInstanceExtensionsForOpenXR(m_xrInstance, systemId);
    for (const std::string &requestExtension : openXrInstanceExtensionNames) {
        for (const VkExtensionProperties &extensionProperty : instanceExtensionProperties) {
            if (strcmp(requestExtension.c_str(), extensionProperty.extensionName) != 0) {
                continue;
            } else {
                activeInstanceExtensions.push_back(requestExtension.c_str());
            }
            break;
        }
    }

    activeInstanceExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

    if (enableValidation) {
        activeInstanceLayers = {"VK_LAYER_KHRONOS_validation"};
    }

    vk::InstanceCreateInfo instanceCI;
    instanceCI.pNext = nullptr; // this was commented out
    if (enableValidation) {
        VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
        populateDebugMessengerCreateInfo(debugCreateInfo);
        instanceCI.pNext = &debugCreateInfo;
    }
    instanceCI.flags = {};
    instanceCI.pApplicationInfo = &ai;
    instanceCI.enabledLayerCount = static_cast<uint32_t>(activeInstanceLayers.size());
    instanceCI.ppEnabledLayerNames = activeInstanceLayers.data();
    // this might need the debug
    instanceCI.enabledExtensionCount = static_cast<uint32_t>(activeInstanceExtensions.size());
    instanceCI.ppEnabledExtensionNames = activeInstanceExtensions.data();
    mInstance_= vk::createInstance(instanceCI);

    if (enableValidation) {
        setupDebugMessenger();
    }

    // Physical Device
    std::vector<vk::PhysicalDevice> physicalDevices = mInstance_.enumeratePhysicalDevices();

    VkPhysicalDevice physicalDeviceFromXR;
    OPENXR_CHECK(
        xrGetVulkanGraphicsDeviceKHR(m_xrInstance, systemId, mInstance_, &physicalDeviceFromXR),
        "Failed to get Graphics Device for Vulkan."
    )
    auto physicalDeviceFromXR_it = std::find_if(
        physicalDevices.begin(),
        physicalDevices.end(),
        [&](const vk::PhysicalDevice& dev) {
            return dev == vk::PhysicalDevice(physicalDeviceFromXR);
        }
    );
    if (physicalDeviceFromXR_it != physicalDevices.end()) {
        mPhysicalDevice_ = *physicalDeviceFromXR_it;
    } else {
        std::cout << "ERROR: Vulkan: Failed to find PhysicalDevice for OpenXR." << std::endl;
        // Select the first available device.
        mPhysicalDevice_ = physicalDevices[0];
    }

    // Device
    std::vector<VkQueueFamilyProperties> queueFamilyProperties;
    uint32_t queueFamilyPropertiesCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(
        mPhysicalDevice_,
        &queueFamilyPropertiesCount,
        nullptr
    );
    queueFamilyProperties.resize(queueFamilyPropertiesCount);
    vkGetPhysicalDeviceQueueFamilyProperties(
        mPhysicalDevice_,
        &queueFamilyPropertiesCount,
        queueFamilyProperties.data()
    );

    std::vector<vk::DeviceQueueCreateInfo> deviceQueueCIs;
    std::vector<std::vector<float>> queuePriorities;
    queuePriorities.resize(queueFamilyProperties.size());
    deviceQueueCIs.resize(queueFamilyProperties.size());
    for (size_t i = 0; i < deviceQueueCIs.size(); i++) {
        for (size_t j = 0; j < queueFamilyProperties[i].queueCount; j++) {
            queuePriorities[i].push_back(1.0f);
        }

        deviceQueueCIs[i].pNext = nullptr;
        deviceQueueCIs[i].flags = {};
        deviceQueueCIs[i].queueFamilyIndex = static_cast<uint32_t>(i);
        deviceQueueCIs[i].queueCount = queueFamilyProperties[i].queueCount;
        deviceQueueCIs[i].pQueuePriorities = queuePriorities[i].data();

        if (BitwiseCheck(queueFamilyProperties[i].queueFlags, VkQueueFlags(VK_QUEUE_GRAPHICS_BIT))
            && queueFamilyIndex == 0xFFFFFFFF && queueIndex == 0xFFFFFFFF) {
            queueFamilyIndex = static_cast<uint32_t>(i);
            queueIndex = 0;
        }
    }

    std::vector<vk::ExtensionProperties> deviceExtensionProperties = mPhysicalDevice_.enumerateDeviceExtensionProperties();
    const std::vector<std::string> &openXrDeviceExtensionNames = GetDeviceExtensionsForOpenXR(m_xrInstance, systemId);
    for (const std::string &requestExtension : openXrDeviceExtensionNames) {
        for (const VkExtensionProperties &extensionProperty : deviceExtensionProperties) {
            if (strcmp(requestExtension.c_str(), extensionProperty.extensionName) != 0) {
                continue;
            } else {
                activeDeviceExtensions.push_back(requestExtension.c_str());
            }
            break;
        }
    }

    vk::PhysicalDeviceFeatures features = mPhysicalDevice_.getFeatures();

    vk::DeviceCreateInfo deviceCI;
    deviceCI.pNext = nullptr;
    deviceCI.flags = {};
    deviceCI.queueCreateInfoCount = static_cast<uint32_t>(deviceQueueCIs.size());
    deviceCI.pQueueCreateInfos = deviceQueueCIs.data();
    deviceCI.enabledLayerCount = 0;
    deviceCI.ppEnabledLayerNames = nullptr;
    deviceCI.enabledExtensionCount = static_cast<uint32_t>(activeDeviceExtensions.size());
    deviceCI.ppEnabledExtensionNames = activeDeviceExtensions.data();
    deviceCI.pEnabledFeatures = &features;
    mDevice_ = mPhysicalDevice_.createDevice(deviceCI);

    vk::CommandPoolCreateInfo cmdPoolCI;
    cmdPoolCI.pNext = nullptr;
    cmdPoolCI.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer;
    cmdPoolCI.queueFamilyIndex = queueFamilyIndex;
    mCommandPool_ = mDevice_.createCommandPool(cmdPoolCI);

    vk::CommandBufferAllocateInfo allocateInfo;
    allocateInfo.pNext = nullptr;
    allocateInfo.commandPool = mCommandPool_;
    allocateInfo.level = vk::CommandBufferLevel::ePrimary;
    allocateInfo.commandBufferCount = 1;
    cmdBuffer = mDevice_.allocateCommandBuffers(allocateInfo)[0];

    mGraphicsQueue_ = mDevice_.getQueue(queueFamilyIndex, queueIndex);

    vk::FenceCreateInfo fenceCI{};
    fenceCI.pNext = nullptr;
    fenceCI.flags = vk::FenceCreateFlagBits::eSignaled;
    fence = mDevice_.createFence(fenceCI);

    uint32_t maxSets = 1024;

    std::vector<vk::DescriptorPoolSize> poolSizes{
        {vk::DescriptorType::eSampler, 16 * maxSets},
        {vk::DescriptorType::eSampledImage, 16 * maxSets},
        {vk::DescriptorType::eStorageImage, 16 * maxSets},
        {vk::DescriptorType::eUniformBuffer, 16 * maxSets},
        {vk::DescriptorType::eStorageBuffer, 16 * maxSets},
        {vk::DescriptorType::eCombinedImageSampler, 16 * maxSets}
    };

    vk::DescriptorPoolCreateInfo descPoolCI;
    descPoolCI.pNext = nullptr;
    descPoolCI.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet;
    descPoolCI.maxSets = maxSets;
    descPoolCI.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    descPoolCI.pPoolSizes = poolSizes.data();
    mDescriptorPool_ = mDevice_.createDescriptorPool(descPoolCI);

    mWorldLockedCameraUniform_ = std::make_unique<UniformBuffer>(
        *this,
        sizeof(glm::mat4) * 2,
        nullptr
    );

    mHeadLockedCameraUniform_ = std::make_unique<UniformBuffer>(
        *this,
        sizeof(glm::mat4) * 2,
        nullptr
    );
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


void GraphicsContextXR::setupDebugMessenger() {
    VkDebugUtilsMessengerCreateInfoEXT createInfo;
    populateDebugMessengerCreateInfo(createInfo);

    if (CreateDebugUtilsMessengerEXT(mInstance_, &createInfo, nullptr, &mDebugMessenger_) != VK_SUCCESS) {
        throw std::runtime_error("failed to set up debug messenger!");
    }
}

void GraphicsContextXR::CreateRenderPass(const std::vector<int64_t>& colorFormats, int64_t depthFormat) {
    // RenderPass
    std::vector<vk::AttachmentDescription> attachmentDescriptions{};
    std::vector<vk::AttachmentReference> colorAttachmentReferences{};
    vk::AttachmentReference depthAttachmentReference;
    for (const auto &colorFormat : colorFormats) {
        vk::AttachmentDescription desc{};
        desc.flags = {};
        desc.format = static_cast<vk::Format>(colorFormat);
        desc.samples = vk::SampleCountFlagBits::e1;
        desc.loadOp = vk::AttachmentLoadOp::eLoad;
        desc.storeOp = vk::AttachmentStoreOp::eStore;
        desc.stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
        desc.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
        desc.initialLayout = vk::ImageLayout::eColorAttachmentOptimal;
        desc.finalLayout = vk::ImageLayout::eColorAttachmentOptimal;

        attachmentDescriptions.emplace_back(desc);
        colorAttachmentReferences.push_back({
            static_cast<uint32_t>(attachmentDescriptions.size() - 1),
            vk::ImageLayout::eColorAttachmentOptimal
        });
    }

    if (depthFormat) {
        vk::AttachmentDescription depthDesc{};
        depthDesc.flags = {};
        depthDesc.format = static_cast<vk::Format>(depthFormat);
        depthDesc.samples = vk::SampleCountFlagBits::e1;
        depthDesc.loadOp = vk::AttachmentLoadOp::eLoad;
        depthDesc.storeOp = vk::AttachmentStoreOp::eStore;
        depthDesc.stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
        depthDesc.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
        depthDesc.initialLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal;
        depthDesc.finalLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal;

        attachmentDescriptions.emplace_back(depthDesc);
        depthAttachmentReference = {
            static_cast<uint32_t>(attachmentDescriptions.size() - 1),
            vk::ImageLayout::eDepthStencilAttachmentOptimal
        };
    }

    vk::SubpassDescription subpassDescription;
    subpassDescription.flags = {};
    subpassDescription.pipelineBindPoint = vk::PipelineBindPoint::eGraphics;
    subpassDescription.inputAttachmentCount = 0;
    subpassDescription.pInputAttachments = nullptr;
    subpassDescription.colorAttachmentCount = static_cast<uint32_t>(colorAttachmentReferences.size());
    subpassDescription.pColorAttachments = colorAttachmentReferences.data();
    subpassDescription.pResolveAttachments = nullptr;
    subpassDescription.pDepthStencilAttachment = depthFormat ? &depthAttachmentReference : nullptr;
    subpassDescription.preserveAttachmentCount = 0;
    subpassDescription.pPreserveAttachments = nullptr;

    vk::SubpassDependency subpassDependency;
    subpassDependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    subpassDependency.dstSubpass = 0;
    subpassDependency.srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
    subpassDependency.dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
    subpassDependency.srcAccessMask = {};
    subpassDependency.dstAccessMask = vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite;
    subpassDependency.dependencyFlags = {};

    vk::RenderPassCreateInfo renderPassCI;
    renderPassCI.pNext = nullptr;
    renderPassCI.flags = {};
    renderPassCI.attachmentCount = static_cast<uint32_t>(attachmentDescriptions.size());
    renderPassCI.pAttachments = attachmentDescriptions.data();
    renderPassCI.subpassCount = 1;
    renderPassCI.pSubpasses = &subpassDescription;
    renderPassCI.dependencyCount = 1;
    renderPassCI.pDependencies = &subpassDependency;
    mRenderPass_ = mDevice_.createRenderPass(renderPassCI);
}

GraphicsContextXR::~GraphicsContextXR() {
    mDevice_.destroy(mDescriptorPool_, nullptr);

    mDevice_.destroy(fence, nullptr);

    mDevice_.freeCommandBuffers(mCommandPool_, { cmdBuffer });
    mDevice_.destroy(mCommandPool_, nullptr);

    mDevice_.destroy(nullptr);
    mInstance_.destroy(nullptr);
}

const XrGraphicsBindingVulkanKHR* GraphicsContextXR::GetGraphicsBinding() {
    graphicsBinding = {XR_TYPE_GRAPHICS_BINDING_VULKAN_KHR};
    graphicsBinding.instance = mInstance_;
    graphicsBinding.physicalDevice = mPhysicalDevice_;
    graphicsBinding.device = mDevice_;
    graphicsBinding.queueFamilyIndex = queueFamilyIndex;
    graphicsBinding.queueIndex = queueIndex;
    return &graphicsBinding;
}

XrSwapchainImageBaseHeader* GraphicsContextXR::AllocateSwapchainImageData(XrSwapchain swapchain,
                                                                     SwapchainType type,
                                                                     uint32_t count) {
    swapchainImagesMap[swapchain].first = type;
    swapchainImagesMap[swapchain].second.resize(count, {XR_TYPE_SWAPCHAIN_IMAGE_VULKAN_KHR});
    return reinterpret_cast<XrSwapchainImageBaseHeader *>(swapchainImagesMap[swapchain].second.data());
}

vk::ImageView GraphicsContextXR::CreateImageView(const ImageViewCreateInfo &imageViewCI) {
    vk::ImageViewCreateInfo vkImageViewCI;
    vkImageViewCI.pNext = nullptr;
    vkImageViewCI.flags = {};
    vkImageViewCI.image = imageViewCI.image;
    vkImageViewCI.viewType = vk::ImageViewType(imageViewCI.view);
    vkImageViewCI.format = imageViewCI.format;
    vkImageViewCI.components = {VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A};
    vkImageViewCI.subresourceRange.aspectMask = imageViewCI.aspect;
    vkImageViewCI.subresourceRange.baseMipLevel = imageViewCI.baseMipLevel;
    vkImageViewCI.subresourceRange.levelCount = imageViewCI.levelCount;
    vkImageViewCI.subresourceRange.baseArrayLayer = imageViewCI.baseArrayLayer;
    vkImageViewCI.subresourceRange.layerCount = imageViewCI.layerCount;
    vk::ImageView imageView = mDevice_.createImageView(vkImageViewCI, nullptr);

    imageViewResources[imageView] = imageViewCI;
    return imageView;
}

void GraphicsContextXR::DestroyImageView(VkImageView& imageView) {
    vkDestroyImageView(mDevice_, imageView, nullptr);
    imageViewResources.erase(imageView);
    imageView = nullptr;
}

void GraphicsContextXR::BeginRendering() {
    mDevice_.waitForFences(fence, true, UINT64_MAX);
    mDevice_.resetFences(fence);
    for (const auto& descSet : cmdBufferDescriptorSets[cmdBuffer]) {
        mDevice_.freeDescriptorSets(mDescriptorPool_, { descSet });
    }
    cmdBufferDescriptorSets.erase(cmdBuffer);

    for (const vk::Framebuffer& framebuffer : cmdBufferFramebuffers[cmdBuffer]) {
        mDevice_.destroy(framebuffer, nullptr);
    }
    cmdBufferFramebuffers.erase(cmdBuffer);

    cmdBuffer.reset();

    vk::CommandBufferBeginInfo beginInfo;
    beginInfo.pNext = nullptr;
    beginInfo.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;
    beginInfo.pInheritanceInfo = nullptr;
    cmdBuffer.begin(beginInfo);
}

void GraphicsContextXR::EndRendering() {
    if (inRenderPass) {
        vkCmdEndRenderPass(cmdBuffer);
        inRenderPass = false;
    }

    cmdBuffer.end();

    vk::PipelineStageFlags waitDstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;

    vk::SubmitInfo submitInfo{};
    submitInfo.pNext = nullptr;
    submitInfo.waitSemaphoreCount = acquireSemaphore ? 1 : 0;
    submitInfo.pWaitSemaphores = acquireSemaphore ? &acquireSemaphore : nullptr;
    submitInfo.pWaitDstStageMask = acquireSemaphore ? &waitDstStageMask : nullptr;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmdBuffer;
    submitInfo.signalSemaphoreCount = submitSemaphore ? 1 : 0;
    submitInfo.pSignalSemaphores = submitSemaphore ? &submitSemaphore : nullptr;

    mGraphicsQueue_.submit(1, &submitInfo, fence);
}

void GraphicsContextXR::SetBufferData(VkBuffer buffer, size_t offset, size_t size, void *data) {
    VkDeviceMemory memory = bufferResources[buffer].first;
    void* mappedData = mDevice_.mapMemory(memory, offset, size);
    if (mappedData && data) {
        memcpy(mappedData, data, size);
        // Because the VkDeviceMemory use a heap with properties (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
        // We don't need to use vkFlushMappedMemoryRanges() or vkInvalidateMappedMemoryRanges()
    }
    vkUnmapMemory(mDevice_, memory);
}

void GraphicsContextXR::ClearColor(VkImageView imageView, float r, float g, float b, float a) {
    const ImageViewCreateInfo &imageViewCI = imageViewResources[imageView];

    vk::ClearColorValue clearColor;
    clearColor.float32[0] = r;
    clearColor.float32[1] = g;
    clearColor.float32[2] = b;
    clearColor.float32[3] = a;

    vk::ImageSubresourceRange range;
    range.aspectMask = vk::ImageAspectFlagBits::eColor;
    range.baseMipLevel = imageViewCI.baseMipLevel;
    range.levelCount = imageViewCI.levelCount;
    range.baseArrayLayer = imageViewCI.baseArrayLayer;
    range.layerCount = imageViewCI.layerCount;

    vk::Image vkImage = (VkImage)(imageViewCI.image);

    vk::ImageMemoryBarrier imageBarrier;
    imageBarrier.pNext = nullptr;
    imageBarrier.srcAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;
    imageBarrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;
    imageBarrier.oldLayout = vk::ImageLayout::eColorAttachmentOptimal;
    imageBarrier.newLayout = vk::ImageLayout::eTransferDstOptimal;
    imageBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    imageBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    imageBarrier.image = vkImage;
    imageBarrier.subresourceRange = range;
    cmdBuffer.pipelineBarrier(
        vk::PipelineStageFlagBits::eColorAttachmentOutput,
        vk::PipelineStageFlagBits::eTransfer,
        {},
        0,
        nullptr,
        0,
        nullptr,
        1,
        &imageBarrier
    );

    cmdBuffer.clearColorImage(
        vkImage,
        vk::ImageLayout::eTransferDstOptimal,
        clearColor,
        {range}
    );

    imageBarrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
    imageBarrier.dstAccessMask = vk::AccessFlagBits::eColorAttachmentRead;
    imageBarrier.oldLayout = vk::ImageLayout::eTransferDstOptimal;
    imageBarrier.newLayout = vk::ImageLayout::eColorAttachmentOptimal;
    imageBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    imageBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    imageBarrier.image = vkImage;
    imageBarrier.subresourceRange = range;

    cmdBuffer.pipelineBarrier(
        vk::PipelineStageFlagBits::eTransfer,
        vk::PipelineStageFlagBits::eColorAttachmentOutput,
        {},
        0,
        nullptr,
        0,
        nullptr,
        1,
        &imageBarrier
    );
}

void GraphicsContextXR::ClearDepth(VkImageView imageView, float d) {
    const ImageViewCreateInfo &imageViewCI = imageViewResources[(VkImageView)imageView];

    vk::ClearDepthStencilValue clearDepth {
        .depth = d,
        .stencil = 0
    };

    vk::ImageSubresourceRange range {
        .aspectMask = vk::ImageAspectFlagBits::eDepth | vk::ImageAspectFlagBits::eStencil,
        .baseMipLevel = imageViewCI.baseMipLevel,
        .levelCount = imageViewCI.levelCount,
        .baseArrayLayer = imageViewCI.baseArrayLayer,
        .layerCount = imageViewCI.layerCount
    };

    VkImage vkImage = (VkImage)(imageViewCI.image);

    vk::ImageMemoryBarrier imageBarrier;
    imageBarrier.pNext = nullptr;
    imageBarrier.srcAccessMask = {};
    imageBarrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;
    imageBarrier.oldLayout = vk::ImageLayout::eUndefined;
    imageBarrier.newLayout = vk::ImageLayout::eTransferDstOptimal;
    imageBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    imageBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    imageBarrier.image = vkImage;
    imageBarrier.subresourceRange = range;
    cmdBuffer.pipelineBarrier(
        vk::PipelineStageFlagBits::eTopOfPipe,
        vk::PipelineStageFlagBits::eTransfer,
        {},
        0,
        nullptr,
        0,
        nullptr,
        1,
        &imageBarrier
    );

    cmdBuffer.clearDepthStencilImage(vkImage, vk::ImageLayout::eTransferDstOptimal, &clearDepth, 1, &range);

    imageBarrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
    imageBarrier.dstAccessMask = vk::AccessFlagBits::eDepthStencilAttachmentRead;
    imageBarrier.oldLayout = vk::ImageLayout::eTransferDstOptimal;
    imageBarrier.newLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal;
    imageBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    imageBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    imageBarrier.image = vkImage;
    imageBarrier.subresourceRange = range;

    cmdBuffer.pipelineBarrier(
        vk::PipelineStageFlagBits::eTransfer,
        vk::PipelineStageFlagBits::eEarlyFragmentTests,
        {},
        0, nullptr,
        0, nullptr,
        1,
        &imageBarrier
    );
}

void GraphicsContextXR::SetRenderAttachments(VkImageView* colorViews,
                                        size_t colorViewCount,
                                        VkImageView depthStencilView,
                                        uint32_t width,
                                        uint32_t height) {
    if (inRenderPass) {
        cmdBuffer.endRenderPass();
    }

    std::vector<vk::ImageView> vkImageViews;
    for (size_t i = 0; i < colorViewCount; i++) {
        vkImageViews.push_back(colorViews[i]);
    }
    if (depthStencilView) {
        vkImageViews.push_back((vk::ImageView)depthStencilView);
    }

    vk::FramebufferCreateInfo framebufferCI;
    framebufferCI.pNext = nullptr;
    framebufferCI.flags = {};
    framebufferCI.renderPass = mRenderPass_;
    framebufferCI.attachmentCount = static_cast<uint32_t>(vkImageViews.size());
    framebufferCI.pAttachments =  vkImageViews.data();
    framebufferCI.width = width;
    framebufferCI.height = height;
    framebufferCI.layers = 1;

    vk::Framebuffer framebuffer = mDevice_.createFramebuffer(framebufferCI, nullptr);
    cmdBufferFramebuffers[cmdBuffer].push_back(framebuffer);

    vk::RenderPassBeginInfo renderPassBegin;
    renderPassBegin.pNext = nullptr;
    renderPassBegin.renderPass = mRenderPass_;
    renderPassBegin.framebuffer = framebuffer;
    renderPassBegin.renderArea.offset = vk::Offset2D{0, 0};
    renderPassBegin.renderArea.extent = vk::Extent2D{
        framebufferCI.width,
        framebufferCI.height
    };
    renderPassBegin.clearValueCount = 0;
    renderPassBegin.pClearValues = nullptr;
    cmdBuffer.beginRenderPass(renderPassBegin, vk::SubpassContents::eInline);
    inRenderPass = true;
}

void GraphicsContextXR::SetViewports(Viewport *viewports, size_t count) {
    std::vector<VkViewport> vkViewports;
    vkViewports.reserve(count);
    for (size_t i = 0; i < count; i++) {
        const Viewport &viewport = viewports[i];
        vkViewports.push_back({
                                  viewport.x,
                                  viewport.y,
                                  viewport.width,
                                  viewport.height,
                                  viewport.minDepth,
                                  viewport.maxDepth
                              });
    }

    vkCmdSetViewport(cmdBuffer, 0, static_cast<uint32_t>(vkViewports.size()), vkViewports.data());
}
void GraphicsContextXR::SetScissors(Rect2D *scissors, size_t count) {
    std::vector<vk::Rect2D> vkRect2D;
    vkRect2D.reserve(count);
    for (size_t i = 0; i < count; i++) {
        const Rect2D &scissor = scissors[i];
        vkRect2D.push_back({
            {scissor.offset.x, scissor.offset.y},
            {scissor.extent.width, scissor.extent.height}
        });
    }

    cmdBuffer.setScissor(0, static_cast<uint32_t>(vkRect2D.size()), vkRect2D.data());
}
void GraphicsContextXR::SetPipeline(VkPipeline pipeline) {
    vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, (VkPipeline)pipeline);
    setPipeline = (VkPipeline)pipeline;
}

void GraphicsContextXR::SetDescriptor(const DescriptorInfo &descriptorInfo) {
    vk::WriteDescriptorSet writeDescSet;
    writeDescSet.pNext = nullptr;
    writeDescSet.dstSet = nullptr;
    writeDescSet.dstBinding = descriptorInfo.bindingIndex;
    writeDescSet.dstArrayElement = 0;
    writeDescSet.descriptorCount = 1;
    writeDescSet.descriptorType = ToVkDescriptorType2(descriptorInfo);
    writeDescSet.pImageInfo = nullptr;
    writeDescSet.pBufferInfo = nullptr;
    writeDescSet.pTexelBufferView = nullptr;
    writeDescSets.push_back({writeDescSet, {}, {}});

    if (descriptorInfo.type == DescriptorInfo::Type::BUFFER) {
        VkDescriptorBufferInfo &descBufferInfo = std::get<1>(writeDescSets.back());
        VkBuffer buffer = (VkBuffer)descriptorInfo.resource;
        const BufferCreateInfo& bufferCI = bufferResources[buffer].second;
        descBufferInfo.buffer = buffer;
        descBufferInfo.offset = descriptorInfo.bufferOffset;
        descBufferInfo.range = descriptorInfo.bufferSize;
    } else if (descriptorInfo.type == DescriptorInfo::Type::IMAGE) {
        VkDescriptorImageInfo& descImageInfo = std::get<2>(writeDescSets.back());
        VkImageView imageView = (VkImageView)descriptorInfo.resource;
        descImageInfo.sampler = VK_NULL_HANDLE;
        descImageInfo.imageView = imageView;
        descImageInfo.imageLayout = descriptorInfo.readWrite ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    } else if (descriptorInfo.type == DescriptorInfo::Type::SAMPLER) {
        VkDescriptorImageInfo& descImageInfo = std::get<2>(writeDescSets.back());
        VkSampler sampler = (VkSampler)descriptorInfo.resource;
        descImageInfo.sampler = sampler;
        descImageInfo.imageView = VK_NULL_HANDLE;
        descImageInfo.imageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    } else {
        std::cout << "Unknown Descriptor Type" << std::endl;
        DEBUG_BREAK;
        return;
    }
}

void GraphicsContextXR::UpdateDescriptors() {
    vk::PipelineLayout pipelineLayout = std::get<0>(pipelineResources[(VkPipeline)setPipeline]);
    vk::DescriptorSetLayout descSetLayout = std::get<1>(pipelineResources[(VkPipeline)setPipeline]);
    PipelineCreateInfo pipelinCI = std::get<2>(pipelineResources[(VkPipeline)setPipeline]);

    vk::DescriptorSetAllocateInfo descSetAI;
    descSetAI.pNext = nullptr;
    descSetAI.descriptorPool = mDescriptorPool_;
    descSetAI.descriptorSetCount = 1;
    descSetAI.pSetLayouts = &descSetLayout;
    vk::DescriptorSet descSet = mDevice_.allocateDescriptorSets(descSetAI)[0];

    std::vector<vk::WriteDescriptorSet> vkWriteDescSets;
    for (auto& writeDescSet : writeDescSets) {
        vk::WriteDescriptorSet& vkWriteDescSet = std::get<0>(writeDescSet);
        vk::DescriptorBufferInfo &vkDescBufferInfo = std::get<1>(writeDescSet);
        vk::DescriptorImageInfo &vkDescImageInfo = std::get<2>(writeDescSet);

        vkWriteDescSet.dstSet = descSet;
        if (vkDescBufferInfo.buffer) {
            vkWriteDescSet.pBufferInfo = &vkDescBufferInfo;
        } else if (vkDescImageInfo.imageView || vkDescImageInfo.sampler) {
            vkWriteDescSet.pImageInfo = &vkDescImageInfo;
        } else {
            continue;
        }
        vkWriteDescSets.push_back(vkWriteDescSet);
    }
    mDevice_.updateDescriptorSets(
        static_cast<uint32_t>(vkWriteDescSets.size()),
        vkWriteDescSets.data(),
        0,
        nullptr
        );
    writeDescSets.clear();

    cmdBuffer.bindDescriptorSets(
        vk::PipelineBindPoint::eGraphics,
        pipelineLayout,
        0,
        1,
        &descSet,
        0,
        nullptr
    );
    cmdBufferDescriptorSets[cmdBuffer].push_back({descSet});
}

void GraphicsContextXR::SetVertexBuffers(VkBuffer* vertexBuffers, size_t count) {
    std::vector<VkBuffer> vkBuffers;
    std::vector<VkDeviceSize> offsets;
    for (size_t i = 0; i < count; i++) {
        vkBuffers.push_back((VkBuffer)vertexBuffers[i]);
        offsets.push_back(0);
    }

    vkCmdBindVertexBuffers(
        cmdBuffer,
        0,
        static_cast<uint32_t>(vkBuffers.size()),
        vkBuffers.data(),
        offsets.data()
    );
}

void GraphicsContextXR::SetIndexBuffer(VkBuffer indexBuffer) {
    const BufferCreateInfo &bufferCI = bufferResources[(VkBuffer)indexBuffer].second;
    VkIndexType type = bufferCI.stride == 4 ? VK_INDEX_TYPE_UINT32 : VK_INDEX_TYPE_UINT16;
    vkCmdBindIndexBuffer(cmdBuffer, (VkBuffer)indexBuffer, 0, type);
}

void GraphicsContextXR::DrawIndexed(uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t vertexOffset, uint32_t firstInstance) {
    vkCmdDrawIndexed(cmdBuffer, indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
}

void GraphicsContextXR::Draw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance) {
    vkCmdDraw(cmdBuffer, vertexCount, instanceCount, firstVertex, firstInstance);
}

void GraphicsContextXR::LoadPFN_XrFunctions(XrInstance m_xrInstance) {
    OPENXR_CHECK(
        xrGetInstanceProcAddr(
            m_xrInstance,
            "xrGetVulkanGraphicsRequirementsKHR",
            (PFN_xrVoidFunction *)&xrGetVulkanGraphicsRequirementsKHR
        ),
        "Failed to get InstanceProcAddr for xrGetVulkanGraphicsRequirementsKHR."
    )
    OPENXR_CHECK(
        xrGetInstanceProcAddr(
            m_xrInstance,
            "xrGetVulkanInstanceExtensionsKHR",
            (PFN_xrVoidFunction *)&xrGetVulkanInstanceExtensionsKHR
        ),
        "Failed to get InstanceProcAddr for xrGetVulkanInstanceExtensionsKHR."
    )
    OPENXR_CHECK(
        xrGetInstanceProcAddr(
            m_xrInstance,
            "xrGetVulkanDeviceExtensionsKHR",
            (PFN_xrVoidFunction *)&xrGetVulkanDeviceExtensionsKHR
        ),
        "Failed to get InstanceProcAddr for xrGetVulkanDeviceExtensionsKHR."
    )
    OPENXR_CHECK(
        xrGetInstanceProcAddr(
            m_xrInstance,
            "xrGetVulkanGraphicsDeviceKHR",
            (PFN_xrVoidFunction *)&xrGetVulkanGraphicsDeviceKHR
        ),
        "Failed to get InstanceProcAddr for xrGetVulkanGraphicsDeviceKHR."
    )
}

std::vector<std::string> GraphicsContextXR::GetInstanceExtensionsForOpenXR(XrInstance m_xrInstance, XrSystemId systemId) {
    uint32_t extensionNamesSize = 0;
    OPENXR_CHECK(
        xrGetVulkanInstanceExtensionsKHR(m_xrInstance, systemId, 0, &extensionNamesSize, nullptr),
        "Failed to get Vulkan Instance Extensions."
    )

    std::vector<char> extensionNames(extensionNamesSize);
    OPENXR_CHECK(
        xrGetVulkanInstanceExtensionsKHR(m_xrInstance, systemId, extensionNamesSize, &extensionNamesSize, extensionNames.data()),
        "Failed to get Vulkan Instance Extensions."
    )

    std::stringstream streamData(extensionNames.data());
    std::vector<std::string> extensions;
    std::string extension;
    while (std::getline(streamData, extension, ' ')) {
        extensions.push_back(extension);
    }
    return extensions;
}

std::vector<std::string> GraphicsContextXR::GetDeviceExtensionsForOpenXR(XrInstance m_xrInstance, XrSystemId systemId) {
    uint32_t extensionNamesSize = 0;
    OPENXR_CHECK(
        xrGetVulkanDeviceExtensionsKHR(m_xrInstance, systemId, 0, &extensionNamesSize, nullptr),
        "Failed to get Vulkan Device Extensions."
    )

    std::vector<char> extensionNames(extensionNamesSize);
    OPENXR_CHECK(xrGetVulkanDeviceExtensionsKHR(
                     m_xrInstance, systemId, extensionNamesSize, &extensionNamesSize, extensionNames.data()),
                 "Failed to get Vulkan Device Extensions."
    )

    std::stringstream streamData(extensionNames.data());
    std::vector<std::string> extensions;
    std::string extension;
    while (std::getline(streamData, extension, ' ')) {
        extensions.push_back(extension);
    }
    return extensions;
}

const std::vector<int64_t> GraphicsContextXR::GetSupportedColorSwapchainFormats() {
    // VkFormat?
    return {
        VK_FORMAT_B8G8R8A8_SRGB,
        VK_FORMAT_R8G8B8A8_SRGB,
        VK_FORMAT_B8G8R8A8_UNORM,
        VK_FORMAT_R8G8B8A8_UNORM
    };
}
const std::vector<int64_t> GraphicsContextXR::GetSupportedDepthSwapchainFormats() {
    return {
        VK_FORMAT_D24_UNORM_S8_UINT,
        //VK_FORMAT_D32_SFLOAT,
        //VK_FORMAT_D16_UNORM
    };
}

void GraphicsContextXR::FreeSwapchainImageData(XrSwapchain swapchain) {
    swapchainImagesMap[swapchain].second.clear();
    swapchainImagesMap.erase(swapchain);
}

XrSwapchainImageBaseHeader* GraphicsContextXR::GetSwapchainImageData(XrSwapchain swapchain, uint32_t index)  {
    return (XrSwapchainImageBaseHeader*)&swapchainImagesMap[swapchain].second[index];
}

VkImage GraphicsContextXR::GetSwapchainImage(XrSwapchain swapchain, uint32_t index) {
    VkImage image = swapchainImagesMap[swapchain].second[index].image;
    VkImageLayout layout = swapchainImagesMap[swapchain].first == SwapchainType::COLOR ? VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    imageStates[image] = layout;
    return image;
}

} // namespace clay

#endif //CLAY_PLATFORM_XR
