#pragma once
#ifdef CLAY_PLATFORM_XR

#define XR_USE_GRAPHICS_API_VULKAN
#define XR_USE_PLATFORM_ANDROID
#define VK_USE_PLATFORM_ANDROID_KHR

#include <android_native_app_glue.h>
#include <vulkan/vulkan.h>
// OpenXR Helper
#include "clay/graphics/common/BaseGraphicsContext.h"
#include "clay/utils/xr/UtilsXR.h"

namespace clay {

class GraphicsContextXR : public BaseGraphicsContext {
public:
    // Pipeline Helpers
    enum class SwapchainType : uint8_t {
        COLOR,
        DEPTH
    };
    struct StencilOpState {
        VkStencilOp failOp;
        VkStencilOp passOp;
        VkStencilOp depthFailOp;
        VkCompareOp compareOp;
        uint32_t compareMask;
        uint32_t writeMask;
        uint32_t reference;
    };
    struct ColorBlendAttachmentState {
        bool blendEnable;
        VkBlendFactor srcColorBlendFactor;
        VkBlendFactor dstColorBlendFactor;
        VkBlendOp colorBlendOp;
        VkBlendFactor srcAlphaBlendFactor;
        VkBlendFactor dstAlphaBlendFactor;
        VkBlendOp alphaBlendOp;
        VkColorComponentFlags colorWriteMask;
    };

    struct VertexInputAttribute {
        uint32_t attribIndex;   // layout(location = X)
        uint32_t bindingIndex;  // Which buffer to use when bound for draws.
        VkFormat vertexType;
        size_t offset;
        const char *semanticName;
    };
    typedef std::vector <VertexInputAttribute> VertexInputAttributes;
    struct VertexInputBinding {
        uint32_t bindingIndex;  // Which buffer to use when bound for draws.
        size_t offset;
        size_t stride;
    };
    typedef std::vector <VertexInputBinding> VertexInputBindings;
    struct VertexInputState {
        VertexInputAttributes attributes;
        VertexInputBindings bindings;
    };
    struct InputAssemblyState {
        VkPrimitiveTopology topology;
        bool primitiveRestartEnable;
    };
    struct RasterisationState {
        bool depthClampEnable;
        bool rasteriserDiscardEnable;
        VkPolygonMode polygonMode;
        VkCullModeFlags cullMode;
        VkFrontFace frontFace;
        bool depthBiasEnable;
        float depthBiasConstantFactor;
        float depthBiasClamp;
        float depthBiasSlopeFactor;
        float lineWidth;
    };
    struct MultisampleState {
        uint32_t rasterisationSamples;
        bool sampleShadingEnable;
        float minSampleShading;
        uint32_t sampleMask;
        bool alphaToCoverageEnable;
        bool alphaToOneEnable;
    };
    struct DepthStencilState {
        bool depthTestEnable;
        bool depthWriteEnable;
        VkCompareOp depthCompareOp;
        bool depthBoundsTestEnable;
        bool stencilTestEnable;
        StencilOpState front;
        StencilOpState back;
        float minDepthBounds;
        float maxDepthBounds;
    };
    struct ColorBlendState {
        bool logicOpEnable;
        VkLogicOp logicOp;
        std::vector <ColorBlendAttachmentState> attachments;
        float blendConstants[4];
    };

    struct DescriptorInfo {
        uint32_t bindingIndex;
        void *resource;
        enum class Type : uint8_t {
            BUFFER,
            IMAGE,
            SAMPLER
        } type;
        VkShaderStageFlagBits stage;
        bool readWrite;
        size_t bufferOffset;
        size_t bufferSize;
    };
    struct PushConstantInfo {
        VkShaderStageFlags stageFlags;
        uint32_t offset;
        uint32_t size;
        void *data; // pointer to your constant data
    };
    struct PipelineCreateInfo {
        std::vector <VkShaderModule> shaders;
        VertexInputState vertexInputState;
        InputAssemblyState inputAssemblyState;
        RasterisationState rasterisationState;
        MultisampleState multisampleState;
        DepthStencilState depthStencilState;
        ColorBlendState colorBlendState;
        std::vector <int64_t> colorFormats; // TODO VkFormat
        int64_t depthFormat; // TODO VkFormat?
        std::vector <DescriptorInfo> layout;
        PushConstantInfo pushConstantInfo;
    };

    struct SwapchainCreateInfo {
        uint32_t width;
        uint32_t height;
        uint32_t count;
        void *windowHandle;
        int64_t format;
        bool vsync;
    };

    struct ImageCreateInfo {
        uint32_t dimension;
        uint32_t width;
        uint32_t height;
        uint32_t depth;
        uint32_t mipLevels;
        uint32_t arrayLayers;
        uint32_t sampleCount;
        int64_t format;
        bool cubemap;
        bool colorAttachment;
        bool depthAttachment;
        bool sampled;
    };

    struct ImageViewCreateInfo {
        VkImage image;
        enum class Type : uint8_t {
            RTV, //  Render Target View
            DSV, // Depth Stencil View
            SRV, //  Shader Resource View
            UAV // Unordered Access View
        } type;
        VkImageViewType view;
        VkFormat format;
        VkImageAspectFlagBits aspect;
        uint32_t baseMipLevel;
        uint32_t levelCount;
        uint32_t baseArrayLayer;
        uint32_t layerCount;
    };

    struct SamplerCreateInfo {
        VkFilter magFilter;
        VkFilter minFilter;
        VkSamplerMipmapMode mipmapMode;
        VkSamplerAddressMode addressModeS;
        VkSamplerAddressMode addressModeT;
        VkSamplerAddressMode addressModeR;
        float mipLodBias;
        bool compareEnable;
        VkCompareOp compareOp;
        float minLod;
        float maxLod;
        float borderColor[4];
    };

    struct Viewport {
        float x;
        float y;
        float width;
        float height;
        float minDepth;
        float maxDepth;
    };
    struct Offset2D {
        int32_t x;
        int32_t y;
    };
    struct Extent2D {
        uint32_t width;
        uint32_t height;
    };
    struct Rect2D {
        Offset2D offset;
        Extent2D extent;
    };

    VkDebugUtilsMessengerEXT mDebugMessenger_;
    static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
        VkDebugUtilsMessageTypeFlagsEXT messageType,
        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
        void* pUserData);

    void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo);

    GraphicsContextXR();

    GraphicsContextXR(XrInstance m_xrInstance, XrSystemId systemId);

    ~GraphicsContextXR();

    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);

    void CreateRenderPass(const std::vector <int64_t> &colorFormats, int64_t depthFormat);

    int64_t SelectColorSwapchainFormat(const std::vector <int64_t> &formats);

    int64_t SelectDepthSwapchainFormat(const std::vector <int64_t> &formats);

    int64_t GetDepthFormat() { return (int64_t) VK_FORMAT_D32_SFLOAT; }

    const XrGraphicsBindingVulkanKHR* GetGraphicsBinding();

    XrSwapchainImageBaseHeader *
    AllocateSwapchainImageData(XrSwapchain swapchain, SwapchainType type, uint32_t count);

    void FreeSwapchainImageData(XrSwapchain swapchain);

    XrSwapchainImageBaseHeader *GetSwapchainImageData(XrSwapchain swapchain, uint32_t index);

    VkImage GetSwapchainImage(XrSwapchain swapchain, uint32_t index);

    void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size) override;

    void createImage(uint32_t width,
                    uint32_t height,
                    uint32_t mipLevels,
                    VkSampleCountFlagBits numSamples,
                    VkFormat format,
                    VkImageTiling tiling,
                    VkImageUsageFlags usage,
                    VkMemoryPropertyFlags properties,
                    VkImage& image,
                    VkDeviceMemory& imageMemory) override;

    void transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout, uint32_t mipLevels);

    VkShaderModule createShader(const ShaderCreateInfo& shaderCI) override;

    void BeginRendering();

    void EndRendering();

    void SetBufferData(VkBuffer buffer, size_t offset, size_t size, void *data);

    void ClearColor(VkImageView imageView, float r, float g, float b, float a);

    void ClearDepth(VkImageView imageView, float d);

    void SetRenderAttachments(VkImageView *colorViews, size_t colorViewCount,
                              VkImageView depthStencilView, uint32_t width, uint32_t height);

    void SetViewports(Viewport *viewports, size_t count);

    void SetScissors(Rect2D *scissors, size_t count);

    void SetPipeline(VkPipeline pipeline);

    void SetDescriptor(const DescriptorInfo &descriptorInfo);

    void UpdateDescriptors();

    void SetVertexBuffers(VkBuffer *vertexBuffers, size_t count);

    void SetIndexBuffer(VkBuffer indexBuffer);

    void DrawIndexed(uint32_t indexCount, uint32_t instanceCount = 1, uint32_t firstIndex = 0,
                     int32_t vertexOffset = 0, uint32_t firstInstance = 0);

    void Draw(uint32_t vertexCount, uint32_t instanceCount = 1, uint32_t firstVertex = 0,
              uint32_t firstInstance = 0);

    VkImageView CreateImageView(const ImageViewCreateInfo& imageViewCI);

    void DestroyImageView(VkImageView& imageView);

        //private:
    void LoadPFN_XrFunctions(XrInstance m_xrInstance);

    std::vector <std::string>
    GetInstanceExtensionsForOpenXR(XrInstance m_xrInstance, XrSystemId systemId);

    std::vector <std::string>
    GetDeviceExtensionsForOpenXR(XrInstance m_xrInstance, XrSystemId systemId);

    const std::vector <int64_t> GetSupportedColorSwapchainFormats();

    const std::vector <int64_t> GetSupportedDepthSwapchainFormats();

    void setupDebugMessenger();

    bool checkValidationLayerSupport();


//private:
    // VkInstance mInstance_{};

    uint32_t queueFamilyIndex = 0xFFFFFFFF;
    uint32_t queueIndex = 0xFFFFFFFF;
    VkFence fence{};

    VkCommandBuffer cmdBuffer{};

    std::vector<const char*> activeInstanceLayers{};
    std::vector<const char*> activeInstanceExtensions{};
    std::vector<const char*> activeDeviceLayer{};
    std::vector<const char*> activeDeviceExtensions{};

    PFN_xrGetVulkanGraphicsRequirementsKHR xrGetVulkanGraphicsRequirementsKHR = nullptr;
    PFN_xrGetVulkanInstanceExtensionsKHR xrGetVulkanInstanceExtensionsKHR = nullptr;
    PFN_xrGetVulkanDeviceExtensionsKHR xrGetVulkanDeviceExtensionsKHR = nullptr;
    PFN_xrGetVulkanGraphicsDeviceKHR xrGetVulkanGraphicsDeviceKHR = nullptr;
    XrGraphicsBindingVulkanKHR graphicsBinding{};

    std::unordered_map <XrSwapchain, std::pair<SwapchainType,
        std::vector < XrSwapchainImageVulkanKHR>>> swapchainImagesMap{};

    std::unordered_map <VkSwapchainKHR, VkSurfaceKHR> surfaces;
    VkSemaphore acquireSemaphore{};
    VkSemaphore submitSemaphore{};

    std::unordered_map <VkImage, VkImageLayout> imageStates;
    std::unordered_map <VkImage, std::pair<VkDeviceMemory, ImageCreateInfo>> imageResources;
    std::unordered_map <VkImageView, ImageViewCreateInfo> imageViewResources;

    std::unordered_map <VkBuffer, std::pair<VkDeviceMemory, BufferCreateInfo>> bufferResources;

    std::unordered_map <VkPipeline, std::tuple<VkPipelineLayout, VkDescriptorSetLayout, PipelineCreateInfo>> pipelineResources;

    std::unordered_map <VkCommandBuffer, std::vector<VkFramebuffer>> cmdBufferFramebuffers;
    bool inRenderPass = false;

    VkPipeline setPipeline = VK_NULL_HANDLE;
    std::unordered_map <VkCommandBuffer, std::vector<VkDescriptorSet>> cmdBufferDescriptorSets;
    std::vector <std::tuple<VkWriteDescriptorSet, VkDescriptorBufferInfo, VkDescriptorImageInfo>> writeDescSets;

    VkRenderPass imguiRenderPass = VK_NULL_HANDLE;
};

} // namespace clay

#endif // CLAY_PLATFORM_XR