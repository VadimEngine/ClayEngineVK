#ifdef CLAY_PLATFORM_XR

// third party
#include <glm/glm.hpp>
#include "clay/graphics/xr/GraphicsContextXR.h"

#define VULKAN_CHECK(x, y)                                                                     \
{                                                                                              \
    VkResult result = (x);                                                                     \
    if (result != VK_SUCCESS) {                                                                \
        std::cout << "ERROR: VULKAN: " << std::hex << "0x" << result << std::dec << std::endl; \
        std::cout << "ERROR: VULKAN: " << y << std::endl;                                      \
    }                                                                                          \
}

#define VK_MAKE_API_VERSION(variant, major, minor, patch) VK_MAKE_VERSION(major, minor, patch)

namespace clay {

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

VkDescriptorType ToVkDescrtiptorType2(const GraphicsContextXR::DescriptorInfo &descInfo) {
    VkDescriptorType vkType;
    switch (descInfo.type) {
        default:
        case GraphicsContextXR::DescriptorInfo::Type::BUFFER: {
            vkType = descInfo.readWrite ? VK_DESCRIPTOR_TYPE_STORAGE_BUFFER : VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            break;
        }
        case GraphicsContextXR::DescriptorInfo::Type::IMAGE: {
            vkType = descInfo.readWrite ? VK_DESCRIPTOR_TYPE_STORAGE_IMAGE : VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
            break;
        }
        case GraphicsContextXR::DescriptorInfo::Type::SAMPLER: {
            vkType = VK_DESCRIPTOR_TYPE_SAMPLER;
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
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
    std::cout << layerCount << " extensions supported" << std::endl; // why is this 0?

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

GraphicsContextXR::GraphicsContextXR() {
    // Instance
    VkApplicationInfo ai;
    ai.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    ai.pNext = nullptr;
    ai.pApplicationName = "OpenXR Tutorial - Vulkan";
    ai.applicationVersion = 1;
    ai.pEngineName = "OpenXR Tutorial - Vulkan Engine";
    ai.engineVersion = 1;
    ai.apiVersion = VK_MAKE_API_VERSION(0, 1, 0, 0);

    uint32_t instanceExtensionCount = 0;
    VULKAN_CHECK(vkEnumerateInstanceExtensionProperties(
        nullptr,
        &instanceExtensionCount,
        nullptr
        ),
        "Failed to enumerate InstanceExtensionProperties."
    )

    std::vector<VkExtensionProperties> instanceExtensionProperties;
    instanceExtensionProperties.resize(instanceExtensionCount);
    VULKAN_CHECK(
        vkEnumerateInstanceExtensionProperties(
            nullptr,
            &instanceExtensionCount,
            instanceExtensionProperties.data()
        ),
        "Failed to enumerate InstanceExtensionProperties."
    )
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

    VkInstanceCreateInfo instanceCI;
    instanceCI.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instanceCI.pNext = nullptr;
    instanceCI.flags = 0;
    instanceCI.pApplicationInfo = &ai;
    instanceCI.enabledLayerCount = static_cast<uint32_t>(activeInstanceLayers.size());
    instanceCI.ppEnabledLayerNames = activeInstanceLayers.data();
    instanceCI.enabledExtensionCount = static_cast<uint32_t>(activeInstanceExtensions.size());
    instanceCI.ppEnabledExtensionNames = activeInstanceExtensions.data();
    VULKAN_CHECK(
        vkCreateInstance(&instanceCI, nullptr, &mInstance_),
        "Failed to create Vulkan Instance."
    )

    // Physical Device
    uint32_t physicalDeviceCount = 0;
    std::vector<VkPhysicalDevice> physicalDevices;
    VULKAN_CHECK(
        vkEnumeratePhysicalDevices(mInstance_, &physicalDeviceCount, nullptr),
        "Failed to enumerate PhysicalDevices."
    )
    physicalDevices.resize(physicalDeviceCount);
    VULKAN_CHECK(
        vkEnumeratePhysicalDevices(mInstance_, &physicalDeviceCount, physicalDevices.data()),
        "Failed to enumerate PhysicalDevices."
    )
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

    std::vector<VkDeviceQueueCreateInfo> deviceQueueCIs;
    std::vector<std::vector<float>> queuePriorities;
    queuePriorities.resize(queueFamilyProperties.size());
    deviceQueueCIs.resize(queueFamilyProperties.size());
    for (size_t i = 0; i < deviceQueueCIs.size(); i++) {
        for (size_t j = 0; j < queueFamilyProperties[i].queueCount; j++) {
            queuePriorities[i].push_back(1.0f);
        }

        deviceQueueCIs[i].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        deviceQueueCIs[i].pNext = nullptr;
        deviceQueueCIs[i].flags = 0;
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
    VULKAN_CHECK(
        vkEnumerateDeviceExtensionProperties(
            mPhysicalDevice_,
            nullptr,
            &deviceExtensionCount,
            nullptr
        ),
        "Failed to enumerate DeviceExtensionProperties."
    )
    std::vector<VkExtensionProperties> deviceExtensionProperties;
    deviceExtensionProperties.resize(deviceExtensionCount);

    VULKAN_CHECK(
        vkEnumerateDeviceExtensionProperties(
            mPhysicalDevice_,
            nullptr,
            &deviceExtensionCount,
            deviceExtensionProperties.data()
        ),
        "Failed to enumerate DeviceExtensionProperties."
    )
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

    VkPhysicalDeviceFeatures features;
    vkGetPhysicalDeviceFeatures(mPhysicalDevice_, &features);

    VkDeviceCreateInfo deviceCI{};
    deviceCI.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceCI.pNext = nullptr;
    deviceCI.flags = 0;
    deviceCI.queueCreateInfoCount = static_cast<uint32_t>(deviceQueueCIs.size());
    deviceCI.pQueueCreateInfos = deviceQueueCIs.data();
    //deviceCI.enabledLayerCount = 0;
    //deviceCI.ppEnabledLayerNames = nullptr;
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
    VULKAN_CHECK(
        vkCreateDevice(mPhysicalDevice_, &deviceCI, nullptr, &mDevice_),
        "Failed to create Device."
    )

    VkCommandPoolCreateInfo cmdPoolCI;
    cmdPoolCI.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    cmdPoolCI.pNext = nullptr;
    cmdPoolCI.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    cmdPoolCI.queueFamilyIndex = queueFamilyIndex;
    VULKAN_CHECK(
        vkCreateCommandPool(mDevice_, &cmdPoolCI, nullptr, &mCommandPool_),
        "Failed to create CommandPool."
    )

    VkCommandBufferAllocateInfo allocateInfo;
    allocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocateInfo.pNext = nullptr;
    allocateInfo.commandPool = mCommandPool_;
    allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocateInfo.commandBufferCount = 1;
    VULKAN_CHECK(
        vkAllocateCommandBuffers(mDevice_, &allocateInfo, &cmdBuffer),
        "Failed to allocate CommandBuffers."
    )

    vkGetDeviceQueue(mDevice_, queueFamilyIndex, queueIndex, &mGraphicsQueue_);

    VkFenceCreateInfo fenceCI{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    fenceCI.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceCI.pNext = nullptr;
    fenceCI.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    VULKAN_CHECK(
        vkCreateFence(mDevice_, &fenceCI, nullptr, &fence),
        "Failed to create Fence."
    )

    uint32_t maxSets = 1024;
    std::vector<VkDescriptorPoolSize> poolSizes{
        {VK_DESCRIPTOR_TYPE_SAMPLER, 16 * maxSets},
        {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 16 * maxSets},
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 16 * maxSets},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 16 * maxSets},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 16 * maxSets}
    };

    VkDescriptorPoolCreateInfo descPoolCI;
    descPoolCI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    descPoolCI.pNext = nullptr;
    descPoolCI.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    descPoolCI.maxSets = maxSets;
    descPoolCI.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    descPoolCI.pPoolSizes = poolSizes.data();
    VULKAN_CHECK(
        vkCreateDescriptorPool(mDevice_, &descPoolCI, nullptr, &mDescriptorPool_),
        "Failed to create DescriptorPool"
    )
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

    VkApplicationInfo ai;
    ai.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
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
    VULKAN_CHECK(
        vkEnumerateInstanceExtensionProperties(
            nullptr,
            &instanceExtensionCount,
            nullptr
        ),
        "Failed to enumerate InstanceExtensionProperties."
    )

    std::vector<VkExtensionProperties> instanceExtensionProperties;
    instanceExtensionProperties.resize(instanceExtensionCount);
    VULKAN_CHECK(
        vkEnumerateInstanceExtensionProperties(
            nullptr,
            &instanceExtensionCount,
            instanceExtensionProperties.data()
        ),
        "Failed to enumerate InstanceExtensionProperties."
    )
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

    VkInstanceCreateInfo instanceCI;
    instanceCI.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    //instanceCI.pNext = nullptr;
    if (enableValidation) {
        VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
        populateDebugMessengerCreateInfo(debugCreateInfo);
        instanceCI.pNext = &debugCreateInfo;
    }
    instanceCI.flags = 0;
    instanceCI.pApplicationInfo = &ai;
    instanceCI.enabledLayerCount = static_cast<uint32_t>(activeInstanceLayers.size());
    instanceCI.ppEnabledLayerNames = activeInstanceLayers.data();
    // this might need the debug
    instanceCI.enabledExtensionCount = static_cast<uint32_t>(activeInstanceExtensions.size());
    instanceCI.ppEnabledExtensionNames = activeInstanceExtensions.data();
    VULKAN_CHECK(
        vkCreateInstance(&instanceCI, nullptr, &mInstance_),
        "Failed to create Vulkan Instance."
    )

    if (enableValidation) {
        setupDebugMessenger();
    }

    // Physical Device
    uint32_t physicalDeviceCount = 0;
    std::vector<VkPhysicalDevice> physicalDevices;
    VULKAN_CHECK(
        vkEnumeratePhysicalDevices(mInstance_, &physicalDeviceCount, nullptr),
        "Failed to enumerate PhysicalDevices."
    )
    physicalDevices.resize(physicalDeviceCount);
    VULKAN_CHECK(
        vkEnumeratePhysicalDevices(mInstance_, &physicalDeviceCount, physicalDevices.data()),
        "Failed to enumerate PhysicalDevices."
    )

    VkPhysicalDevice physicalDeviceFromXR;
    OPENXR_CHECK(
        xrGetVulkanGraphicsDeviceKHR(m_xrInstance, systemId, mInstance_, &physicalDeviceFromXR),
        "Failed to get Graphics Device for Vulkan."
    )
    auto physicalDeviceFromXR_it = std::find(physicalDevices.begin(), physicalDevices.end(), physicalDeviceFromXR);
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

    std::vector<VkDeviceQueueCreateInfo> deviceQueueCIs;
    std::vector<std::vector<float>> queuePriorities;
    queuePriorities.resize(queueFamilyProperties.size());
    deviceQueueCIs.resize(queueFamilyProperties.size());
    for (size_t i = 0; i < deviceQueueCIs.size(); i++) {
        for (size_t j = 0; j < queueFamilyProperties[i].queueCount; j++) {
            queuePriorities[i].push_back(1.0f);
        }

        deviceQueueCIs[i].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        deviceQueueCIs[i].pNext = nullptr;
        deviceQueueCIs[i].flags = 0;
        deviceQueueCIs[i].queueFamilyIndex = static_cast<uint32_t>(i);
        deviceQueueCIs[i].queueCount = queueFamilyProperties[i].queueCount;
        deviceQueueCIs[i].pQueuePriorities = queuePriorities[i].data();

        if (BitwiseCheck(queueFamilyProperties[i].queueFlags, VkQueueFlags(VK_QUEUE_GRAPHICS_BIT))
            && queueFamilyIndex == 0xFFFFFFFF && queueIndex == 0xFFFFFFFF) {
            queueFamilyIndex = static_cast<uint32_t>(i);
            queueIndex = 0;
        }
    }

    uint32_t deviceExtensionCount = 0;
    VULKAN_CHECK(
        vkEnumerateDeviceExtensionProperties(
            mPhysicalDevice_,
            nullptr,
            &deviceExtensionCount,
            nullptr
        ),
        "Failed to enumerate DeviceExtensionProperties."
    )
    std::vector<VkExtensionProperties> deviceExtensionProperties;
    deviceExtensionProperties.resize(deviceExtensionCount);

    VULKAN_CHECK(
        vkEnumerateDeviceExtensionProperties(
            mPhysicalDevice_,
            nullptr,
            &deviceExtensionCount,
            deviceExtensionProperties.data()
        ),
        "Failed to enumerate DeviceExtensionProperties."
    )
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

    VkPhysicalDeviceFeatures features;
    vkGetPhysicalDeviceFeatures(mPhysicalDevice_, &features);

    VkDeviceCreateInfo deviceCI;
    deviceCI.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceCI.pNext = nullptr;
    deviceCI.flags = 0;
    deviceCI.queueCreateInfoCount = static_cast<uint32_t>(deviceQueueCIs.size());
    deviceCI.pQueueCreateInfos = deviceQueueCIs.data();
    deviceCI.enabledLayerCount = 0;
    deviceCI.ppEnabledLayerNames = nullptr;
    deviceCI.enabledExtensionCount = static_cast<uint32_t>(activeDeviceExtensions.size());
    deviceCI.ppEnabledExtensionNames = activeDeviceExtensions.data();
    deviceCI.pEnabledFeatures = &features;
    VULKAN_CHECK(
        vkCreateDevice(mPhysicalDevice_, &deviceCI, nullptr, &mDevice_),
        "Failed to create Device."
    )

    VkCommandPoolCreateInfo cmdPoolCI;
    cmdPoolCI.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    cmdPoolCI.pNext = nullptr;
    cmdPoolCI.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    cmdPoolCI.queueFamilyIndex = queueFamilyIndex;
    VULKAN_CHECK(
        vkCreateCommandPool(mDevice_, &cmdPoolCI, nullptr, &mCommandPool_),
        "Failed to create CommandPool."
    )

    VkCommandBufferAllocateInfo allocateInfo;
    allocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocateInfo.pNext = nullptr;
    allocateInfo.commandPool = mCommandPool_;
    allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocateInfo.commandBufferCount = 1;
    VULKAN_CHECK(
        vkAllocateCommandBuffers(mDevice_, &allocateInfo, &cmdBuffer),
        "Failed to allocate CommandBuffers."
    )

    vkGetDeviceQueue(mDevice_, queueFamilyIndex, queueIndex, &mGraphicsQueue_);

    VkFenceCreateInfo fenceCI{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    fenceCI.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceCI.pNext = nullptr;
    fenceCI.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    VULKAN_CHECK(
        vkCreateFence(mDevice_, &fenceCI, nullptr, &fence),
        "Failed to create Fence."
    )

    uint32_t maxSets = 1024;
    std::vector<VkDescriptorPoolSize> poolSizes{
        {VK_DESCRIPTOR_TYPE_SAMPLER, 16 * maxSets},
        {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 16 * maxSets},
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 16 * maxSets},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 16 * maxSets},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 16 * maxSets},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 16 * maxSets},
    };

    VkDescriptorPoolCreateInfo descPoolCI;
    descPoolCI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    descPoolCI.pNext = nullptr;
    descPoolCI.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    descPoolCI.maxSets = maxSets;
    descPoolCI.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    descPoolCI.pPoolSizes = poolSizes.data();
    VULKAN_CHECK(
        vkCreateDescriptorPool(mDevice_, &descPoolCI, nullptr, &mDescriptorPool_),
        "Failed to create DescriptorPool"
    )

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
    std::vector<VkAttachmentDescription> attachmentDescriptions{};
    std::vector<VkAttachmentReference> colorAttachmentReferences{};
    VkAttachmentReference depthAttachmentReference;
    for (const auto &colorFormat : colorFormats) {
        attachmentDescriptions.push_back({
            static_cast<VkAttachmentDescriptionFlags>(0),
            static_cast<VkFormat>(colorFormat),
            static_cast<VkSampleCountFlagBits>(1),
            VK_ATTACHMENT_LOAD_OP_LOAD,
            VK_ATTACHMENT_STORE_OP_STORE,
            VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            VK_ATTACHMENT_STORE_OP_DONT_CARE,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        });
        colorAttachmentReferences.push_back({
            static_cast<uint32_t>(attachmentDescriptions.size() - 1),
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
        });
    }

    if (depthFormat) {
        attachmentDescriptions.push_back({
            static_cast<VkAttachmentDescriptionFlags>(0),
            static_cast<VkFormat>(depthFormat),
            static_cast<VkSampleCountFlagBits>(1),
            VK_ATTACHMENT_LOAD_OP_LOAD,
            VK_ATTACHMENT_STORE_OP_STORE,
            VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            VK_ATTACHMENT_STORE_OP_DONT_CARE,
            VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
            VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        });
        depthAttachmentReference = {
            static_cast<uint32_t>(attachmentDescriptions.size() - 1),
            VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
        };
    }

    VkSubpassDescription subpassDescription;
    subpassDescription.flags = static_cast<VkSubpassDescriptionFlags>(0);
    subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpassDescription.inputAttachmentCount = 0;
    subpassDescription.pInputAttachments = nullptr;
    subpassDescription.colorAttachmentCount = static_cast<uint32_t>(colorAttachmentReferences.size());
    subpassDescription.pColorAttachments = colorAttachmentReferences.data();
    subpassDescription.pResolveAttachments = nullptr;
    subpassDescription.pDepthStencilAttachment = depthFormat ? &depthAttachmentReference : nullptr;
    subpassDescription.preserveAttachmentCount = 0;
    subpassDescription.pPreserveAttachments = nullptr;

    VkSubpassDependency subpassDependency;
    subpassDependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    subpassDependency.dstSubpass = 0;
    subpassDependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    subpassDependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    subpassDependency.srcAccessMask = VkAccessFlagBits(0);
    subpassDependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    subpassDependency.dependencyFlags = VkDependencyFlagBits(0);

    VkRenderPassCreateInfo renderPassCI;
    renderPassCI.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassCI.pNext = nullptr;
    renderPassCI.flags = 0;
    renderPassCI.attachmentCount = static_cast<uint32_t>(attachmentDescriptions.size());
    renderPassCI.pAttachments = attachmentDescriptions.data();
    renderPassCI.subpassCount = 1;
    renderPassCI.pSubpasses = &subpassDescription;
    renderPassCI.dependencyCount = 1;
    renderPassCI.pDependencies = &subpassDependency;
    VULKAN_CHECK(
        vkCreateRenderPass(mDevice_, &renderPassCI, nullptr, &mRenderPass_),
        "Failed to create RenderPass."
    )
}


GraphicsContextXR::~GraphicsContextXR() {
    vkDestroyDescriptorPool(mDevice_, mDescriptorPool_, nullptr);

    vkDestroyFence(mDevice_, fence, nullptr);

    vkFreeCommandBuffers(mDevice_, mCommandPool_, 1, &cmdBuffer);
    vkDestroyCommandPool(mDevice_, mCommandPool_, nullptr);

    vkDestroyDevice(mDevice_, nullptr);
    vkDestroyInstance(mInstance_, nullptr);
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

VkImageView GraphicsContextXR::CreateImageView(const ImageViewCreateInfo &imageViewCI) {
    VkImageView imageView{};
    VkImageViewCreateInfo vkImageViewCI;
    vkImageViewCI.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    vkImageViewCI.pNext = nullptr;
    vkImageViewCI.flags = 0;
    vkImageViewCI.image = imageViewCI.image;
    vkImageViewCI.viewType = VkImageViewType(imageViewCI.view);
    vkImageViewCI.format = imageViewCI.format;
    vkImageViewCI.components = {VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A};
    vkImageViewCI.subresourceRange.aspectMask = VkImageAspectFlagBits(imageViewCI.aspect);
    vkImageViewCI.subresourceRange.baseMipLevel = imageViewCI.baseMipLevel;
    vkImageViewCI.subresourceRange.levelCount = imageViewCI.levelCount;
    vkImageViewCI.subresourceRange.baseArrayLayer = imageViewCI.baseArrayLayer;
    vkImageViewCI.subresourceRange.layerCount = imageViewCI.layerCount;
    VULKAN_CHECK(vkCreateImageView(mDevice_, &vkImageViewCI, nullptr, &imageView), "Failed to create ImageView.");

    imageViewResources[imageView] = imageViewCI;
    return imageView;
}

void GraphicsContextXR::DestroyImageView(VkImageView& imageView) {
    vkDestroyImageView(mDevice_, imageView, nullptr);
    imageViewResources.erase(imageView);
    imageView = nullptr;
}

void GraphicsContextXR::BeginRendering() {
    VULKAN_CHECK(
        vkWaitForFences(mDevice_, 1, &fence, true, UINT64_MAX),
        "Failed to wait for Fence"
    )
    VULKAN_CHECK(vkResetFences(mDevice_, 1, &fence), "Failed to reset Fence.")

    for (const auto& descSet : cmdBufferDescriptorSets[cmdBuffer]) {
        VULKAN_CHECK(
            vkFreeDescriptorSets(mDevice_, mDescriptorPool_, 1, &descSet),
            "Failed to free DescriptorSet."
        )
    }
    cmdBufferDescriptorSets.erase(cmdBuffer);

    for (const VkFramebuffer& framebuffer : cmdBufferFramebuffers[cmdBuffer]) {
        vkDestroyFramebuffer(mDevice_, framebuffer, nullptr);
    }
    cmdBufferFramebuffers.erase(cmdBuffer);

    VULKAN_CHECK(
        vkResetCommandBuffer(cmdBuffer, VkCommandBufferResetFlagBits(0)),
        "Failed to reset CommandBuffer."
    )

    VkCommandBufferBeginInfo beginInfo;
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.pNext = nullptr;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    beginInfo.pInheritanceInfo = nullptr;
    VULKAN_CHECK(
        vkBeginCommandBuffer(cmdBuffer, &beginInfo),
        "Failed to begin CommandBuffer."
    )
}

void GraphicsContextXR::EndRendering() {
    if (inRenderPass) {
        vkCmdEndRenderPass(cmdBuffer);
        inRenderPass = false;
    }

    VULKAN_CHECK(vkEndCommandBuffer(cmdBuffer), "Failed to end CommandBuffer.")

    VkPipelineStageFlags waitDstStageMask = VkPipelineStageFlagBits::VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

    VkSubmitInfo submitInfo{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.pNext = nullptr;
    submitInfo.waitSemaphoreCount = acquireSemaphore ? 1 : 0;
    submitInfo.pWaitSemaphores = acquireSemaphore ? &acquireSemaphore : nullptr;
    submitInfo.pWaitDstStageMask = acquireSemaphore ? &waitDstStageMask : nullptr;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmdBuffer;
    submitInfo.signalSemaphoreCount = submitSemaphore ? 1 : 0;
    submitInfo.pSignalSemaphores = submitSemaphore ? &submitSemaphore : nullptr;

    VULKAN_CHECK(vkQueueSubmit(mGraphicsQueue_, 1, &submitInfo, fence), "Failed to submit to Queue.")
}

void GraphicsContextXR::SetBufferData(VkBuffer buffer, size_t offset, size_t size, void *data) {
    VkDeviceMemory memory = bufferResources[buffer].first;
    void *mappedData = nullptr;
    VULKAN_CHECK(vkMapMemory(mDevice_, memory, offset, size, 0, &mappedData), "Can not map Buffer.")
    if (mappedData && data) {
        memcpy(mappedData, data, size);
        // Because the VkDeviceMemory use a heap with properties (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
        // We don't need to use vkFlushMappedMemoryRanges() or vkInvalidateMappedMemoryRanges()
    }
    vkUnmapMemory(mDevice_, memory);
}

void GraphicsContextXR::ClearColor(VkImageView imageView, float r, float g, float b, float a) {
    const ImageViewCreateInfo &imageViewCI = imageViewResources[imageView];

    VkClearColorValue clearColor;
    clearColor.float32[0] = r;
    clearColor.float32[1] = g;
    clearColor.float32[2] = b;
    clearColor.float32[3] = a;

    VkImageSubresourceRange range;
    range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    range.baseMipLevel = imageViewCI.baseMipLevel;
    range.levelCount = imageViewCI.levelCount;
    range.baseArrayLayer = imageViewCI.baseArrayLayer;
    range.layerCount = imageViewCI.layerCount;

    VkImage vkImage = (VkImage)(imageViewCI.image);

    VkImageMemoryBarrier imageBarrier;
    imageBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    imageBarrier.pNext = nullptr;
    imageBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    imageBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    imageBarrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    imageBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    imageBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    imageBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    imageBarrier.image = vkImage;
    imageBarrier.subresourceRange = range;
    vkCmdPipelineBarrier(
        cmdBuffer,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VkDependencyFlagBits(0),
        0,
        nullptr,
        0,
        nullptr,
        1,
        &imageBarrier
    );

    vkCmdClearColorImage(
        cmdBuffer,
        vkImage,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        &clearColor,
        1,
        &range
    );

    imageBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    imageBarrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
    imageBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    imageBarrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    imageBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    imageBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    imageBarrier.image = vkImage;
    imageBarrier.subresourceRange = range;
    vkCmdPipelineBarrier(
        cmdBuffer,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        VkDependencyFlagBits(0),
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

    VkClearDepthStencilValue clearDepth;
    clearDepth.depth = d;
    clearDepth.stencil = 0;

    VkImageSubresourceRange range;
    range.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
    range.baseMipLevel = imageViewCI.baseMipLevel;
    range.levelCount = imageViewCI.levelCount;
    range.baseArrayLayer = imageViewCI.baseArrayLayer;
    range.layerCount = imageViewCI.layerCount;

    VkImage vkImage = (VkImage)(imageViewCI.image);

    VkImageMemoryBarrier imageBarrier;
    imageBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    imageBarrier.pNext = nullptr;
    imageBarrier.srcAccessMask = VkAccessFlagBits(0);
    imageBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    imageBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    imageBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    imageBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    imageBarrier.image = vkImage;
    imageBarrier.subresourceRange = range;
    vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VkDependencyFlagBits(0), 0, nullptr, 0, nullptr, 1, &imageBarrier);

    vkCmdClearDepthStencilImage(cmdBuffer, vkImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearDepth, 1, &range);

    imageBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    imageBarrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
    imageBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    imageBarrier.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    imageBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    imageBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    imageBarrier.image = vkImage;
    imageBarrier.subresourceRange = range;
    vkCmdPipelineBarrier(
        cmdBuffer,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
        VkDependencyFlagBits(0),
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
        vkCmdEndRenderPass(cmdBuffer);
    }

    std::vector<VkImageView> vkImageViews;
    for (size_t i = 0; i < colorViewCount; i++) {
        vkImageViews.push_back(colorViews[i]);
    }
    if (depthStencilView) {
        vkImageViews.push_back((VkImageView)depthStencilView);
    }

    VkFramebuffer framebuffer{};
    VkFramebufferCreateInfo framebufferCI;
    framebufferCI.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebufferCI.pNext = nullptr;
    framebufferCI.flags = 0;
    framebufferCI.renderPass = mRenderPass_;
    framebufferCI.attachmentCount = static_cast<uint32_t>(vkImageViews.size());
    framebufferCI.pAttachments = vkImageViews.data();
    framebufferCI.width = width;
    framebufferCI.height = height;
    framebufferCI.layers = 1;
    VULKAN_CHECK(
        vkCreateFramebuffer(mDevice_, &framebufferCI, nullptr, &framebuffer),
        "Failed to create Framebuffer"
    )
    cmdBufferFramebuffers[cmdBuffer].push_back(framebuffer);

    VkRenderPassBeginInfo renderPassBegin;
    renderPassBegin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassBegin.pNext = nullptr;
    renderPassBegin.renderPass = mRenderPass_;
    renderPassBegin.framebuffer = framebuffer;
    renderPassBegin.renderArea.offset = {0, 0};
    renderPassBegin.renderArea.extent.width = framebufferCI.width;
    renderPassBegin.renderArea.extent.height = framebufferCI.height;
    renderPassBegin.clearValueCount = 0;
    renderPassBegin.pClearValues = nullptr;
    vkCmdBeginRenderPass(cmdBuffer, &renderPassBegin, VK_SUBPASS_CONTENTS_INLINE);
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
    std::vector<VkRect2D> vkRect2D;
    vkRect2D.reserve(count);
    for (size_t i = 0; i < count; i++) {
        const Rect2D &scissor = scissors[i];
        vkRect2D.push_back({
                               {
                                   scissor.offset.x,
                                                      scissor.offset.y},
                               {scissor.extent.width, scissor.extent.height}
                           });
    }

    vkCmdSetScissor(cmdBuffer, 0, static_cast<uint32_t>(vkRect2D.size()), vkRect2D.data());
}
void GraphicsContextXR::SetPipeline(VkPipeline pipeline) {
    vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, (VkPipeline)pipeline);
    setPipeline = (VkPipeline)pipeline;
}

void GraphicsContextXR::SetDescriptor(const DescriptorInfo &descriptorInfo) {
    VkWriteDescriptorSet writeDescSet;
    writeDescSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writeDescSet.pNext = nullptr;
    writeDescSet.dstSet = VK_NULL_HANDLE;
    writeDescSet.dstBinding = descriptorInfo.bindingIndex;
    writeDescSet.dstArrayElement = 0;
    writeDescSet.descriptorCount = 1;
    writeDescSet.descriptorType = ToVkDescrtiptorType2(descriptorInfo);
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
    VkPipelineLayout pipelineLayout = std::get<0>(pipelineResources[(VkPipeline)setPipeline]);
    VkDescriptorSetLayout descSetLayout = std::get<1>(pipelineResources[(VkPipeline)setPipeline]);
    PipelineCreateInfo pipelinCI = std::get<2>(pipelineResources[(VkPipeline)setPipeline]);

    VkDescriptorSet descSet{};
    VkDescriptorSetAllocateInfo descSetAI;
    descSetAI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    descSetAI.pNext = nullptr;
    descSetAI.descriptorPool = mDescriptorPool_;
    descSetAI.descriptorSetCount = 1;
    descSetAI.pSetLayouts = &descSetLayout;
    VULKAN_CHECK(
        vkAllocateDescriptorSets(mDevice_, &descSetAI, &descSet),
        "Failed to allocate DescriptorSet."
    )

    std::vector<VkWriteDescriptorSet> vkWriteDescSets;
    for (auto& writeDescSet : writeDescSets) {
        VkWriteDescriptorSet &vkWriteDescSet = std::get<0>(writeDescSet);
        VkDescriptorBufferInfo &vkDescBufferInfo = std::get<1>(writeDescSet);
        VkDescriptorImageInfo &vkDescImageInfo = std::get<2>(writeDescSet);

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
    vkUpdateDescriptorSets(
        mDevice_,
        static_cast<uint32_t>(vkWriteDescSets.size()),
        vkWriteDescSets.data(),
        0,
        nullptr
    );
    writeDescSets.clear();

    vkCmdBindDescriptorSets(
        cmdBuffer,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
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
