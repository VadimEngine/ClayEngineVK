#pragma once
#ifdef CLAY_PLATFORM_XR

#include "clay/graphics/xr/GraphicsContextXR.h" // todo remove this

namespace clay {

class XRSystem {
public:
    // Custom data structure that is used by PollSystemEvents().
    struct AndroidAppState {
        ANativeWindow* nativeWindow = nullptr;
        bool windowInitialized = false;
        bool resumed = false;
    };

    struct SwapchainInfo {
        XrSwapchain swapchain = XR_NULL_HANDLE;
        int64_t swapchainFormat = 0;
        std::vector<VkImageView> imageViews;
    };

    struct RenderLayerInfo {
        XrTime predictedDisplayTime = 0;
        std::vector<XrCompositionLayerBaseHeader*> layers;
        XrCompositionLayerProjection layerProjection = {XR_TYPE_COMPOSITION_LAYER_PROJECTION};
        std::vector<XrCompositionLayerProjectionView> layerProjectionViews;
    };

    // Processes the next command from the Android OS. It updates AndroidAppState.
    static void AndroidAppHandleCmd(struct android_app *app, int32_t cmd);

    XRSystem(android_app* pAndroidApp);

    void initialize();

    void finalize();

    void PollSystemEvents();

    void CreateInstance();

    void DestroyInstance();

    void CreateDebugMessenger();

    void DestroyDebugMessenger();

    void GetInstanceProperties();

    void GetSystemID();

    void GetEnvironmentBlendModes();

    void GetViewConfigurationViews();

    void CreateSession();

    void DestroySession();

    void DestroyResources();

    void PollEvents();

    void CreateReferenceSpace();

    void DestroyReferenceSpace();

    void CreateSwapchains();

    void DestroySwapchains();

    AndroidAppState mAndroidAppState_;
    android_app* mpAndroidApp_ = nullptr;

    XrInstance m_xrInstance = XR_NULL_HANDLE;
    std::vector<const char *> m_activeAPILayers = {};
    std::vector<const char *> m_activeInstanceExtensions = {};
    std::vector<std::string> m_apiLayers = {};
    std::vector<std::string> m_instanceExtensions = {};

    XrDebugUtilsMessengerEXT m_debugUtilsMessenger = XR_NULL_HANDLE;

    XrFormFactor m_formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
    XrSystemId m_systemID = {};

    XrSystemProperties m_systemProperties = {XR_TYPE_SYSTEM_PROPERTIES};

    XrSession m_session = {};
    XrSessionState m_sessionState = XR_SESSION_STATE_UNKNOWN;
    bool m_applicationRunning = true;
    bool m_sessionRunning = false;

    std::vector<XrViewConfigurationType> m_applicationViewConfigurations = {XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_MONO};
    std::vector<XrViewConfigurationType> m_viewConfigurations;
    XrViewConfigurationType m_viewConfiguration = XR_VIEW_CONFIGURATION_TYPE_MAX_ENUM;
    std::vector<XrViewConfigurationView> m_viewConfigurationViews;

    std::vector<SwapchainInfo> m_colorSwapchainInfos = {};
    std::vector<SwapchainInfo> m_depthSwapchainInfos = {};

    std::vector<XrEnvironmentBlendMode> m_applicationEnvironmentBlendModes = {XR_ENVIRONMENT_BLEND_MODE_OPAQUE, XR_ENVIRONMENT_BLEND_MODE_ADDITIVE};
    std::vector<XrEnvironmentBlendMode> m_environmentBlendModes = {};
    XrEnvironmentBlendMode m_environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_MAX_ENUM;

    XrSpace m_localSpace = XR_NULL_HANDLE;
    XrSpace mHeadSpace_ = XR_NULL_HANDLE;

    // todo for now, have this make the graphics context but pass it to app
    GraphicsContextXR* mpGraphicsContext_ = nullptr;
};

} // namespace clay

#endif // CLAY_PLATFORM_XR