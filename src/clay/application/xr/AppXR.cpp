#ifdef CLAY_PLATFORM_XR

//clay
#include "clay/utils/common/Logger.h"
#include "clay/gui/xr/ImGuiComponentXR.h"
// class
#include "clay/application/xr/AppXR.h"
// third party
#include <android/asset_manager.h>
#include <android/bitmap.h>
#include <android/imagedecoder.h>

// Declare some useful operators for vecztors:
XrVector3f operator-(XrVector3f a, XrVector3f b) {
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}
XrVector3f operator*(XrVector3f a, float b) {
    return {a.x * b, a.y * b, a.z * b};
}

glm::mat4 ConvertXrMatrixToGlm(const XrMatrix4x4f& xrMat) {
    glm::mat4 result;

    for (int row = 0; row < 4; ++row) {
        for (int col = 0; col < 4; ++col) {
            result[col][row] = xrMat.m[col * 4 + row];
        }
    }

    return result;
}

namespace clay {

//static int32_t handleInputEvent(struct android_app* /*app*/, AInputEvent* inputEvent) {
//    //return ImGui_ImplAndroid_HandleInputEvent(inputEvent);
//}

AppXR::AppXR(android_app* pAndroidApp)
    : mpAndroidApp_(pAndroidApp) {
     mpAndroidApp_->userData = &mAndroidAppState_;
     mpAndroidApp_->onAppCmd = AppXR::AndroidAppHandleCmd;
    //mpAndroidApp_->onInputEvent = handleInputEvent;
}

void AppXR::initialize() {

}

void AppXR::Run() {
    CreateInstance();
    CreateDebugMessenger();
    GetInstanceProperties();
    GetSystemID();
    GetViewConfigurationViews();
    GetEnvironmentBlendModes();
    CreateSession();
    CreateReferenceSpace();
    CreateSwapchains();
    mpGraphicsContextXR_->CreateRenderPass(
        {m_colorSwapchainInfos[0].swapchainFormat},
        m_depthSwapchainInfos[0].swapchainFormat
    );

    mInputHandler_.initialize(m_xrInstance, m_session, m_localSpace, mHeadSpace_);

    // Wait loop: blocks until window is initialized (APP_CMD_TERM_WINDOW)
    while (!mAndroidAppState_.windowInitialized && m_applicationRunning) {
        PollSystemEvents(); // allow state to update
        PollEvents();       // you could sleep briefly here if you want to reduce CPU usage
    }
    ImGuiComponentXR::initialize(mAndroidAppState_.nativeWindow);
    CreateResources(); // Now it's guaranteed that nativeWindow is available

    while (m_applicationRunning) {
        PollSystemEvents();
        PollEvents();
        if (m_sessionRunning) {
            RenderFrame();
        }
    }

    DestroySwapchains();
    DestroyReferenceSpace();
    DestroyResources();
    DestroySession();
    DestroyDebugMessenger();
    DestroyInstance();
}

AAssetManager* AppXR::getAssetManager() {
    return mpAndroidApp_->activity->assetManager;
}

void AppXR::setScene(BaseScene* newScene) {
    mScenes_.emplace_back(newScene);
}

InputHandlerXR& AppXR::getInputHandler() {
    return mInputHandler_;
}

Resources& AppXR::getResources() {
    return *mpResources_;
}

AudioManager& AppXR::getAudioManager() {
    return mAudioManger_;
}

utils::FileData AppXR::loadFileToMemory_XR(const std::string &filePath) {
    auto *assetManager = getAssetManager();
    AAsset *asset = AAssetManager_open(assetManager, filePath.c_str(), AASSET_MODE_BUFFER);
    if (!asset) {
        throw std::runtime_error("Failed to open asset: " + std::string(filePath));
    }
    size_t fileSize = AAsset_getLength(asset);

    auto buffer = std::make_unique<unsigned char[]>(fileSize);
    AAsset_read(asset, buffer.get(), fileSize);
    AAsset_close(asset);

    return {std::move(buffer), static_cast<std::size_t>(fileSize)};
}

utils::ImageData AppXR::loadImageFileToMemory_XR(const std::string &filePath) {
    auto *assetManager = getAssetManager();
    AAsset *asset = AAssetManager_open(assetManager, filePath.c_str(), AASSET_MODE_STREAMING);

    if (!asset) {
        throw std::runtime_error("Failed to open asset file: " + std::string(filePath));
    }

    AImageDecoder *decoder = nullptr;
    int result = AImageDecoder_createFromAAsset(asset, &decoder);
    if (result != ANDROID_IMAGE_DECODER_SUCCESS) {
        AAsset_close(asset);
        throw std::runtime_error("Failed to create image decoder for file: " + filePath);
    }

    utils::ImageData imageData{};

    const AImageDecoderHeaderInfo *info = AImageDecoder_getHeaderInfo(decoder);
    imageData.width = AImageDecoderHeaderInfo_getWidth(info);
    imageData.height = AImageDecoderHeaderInfo_getHeight(info);

    if ((AndroidBitmapFormat) AImageDecoderHeaderInfo_getAndroidBitmapFormat(info) ==
        AndroidBitmapFormat::ANDROID_BITMAP_FORMAT_RGBA_8888) {
        imageData.channels = 4;
    } else {
        throw std::runtime_error("Unsupported Image format: " + filePath);
    }

    const size_t stride = AImageDecoder_getMinimumStride(decoder);
    const size_t size = imageData.height * stride;

    imageData.pixels = std::make_unique<unsigned char[]>(size);

    result = AImageDecoder_decodeImage(decoder, imageData.pixels.get(), stride, size);
    if (result != ANDROID_IMAGE_DECODER_SUCCESS) {
        AImageDecoder_delete(decoder);
        AAsset_close(asset);
        throw std::runtime_error("Failed to decode image from asset.");
    }

    // Cleanup
    AImageDecoder_delete(decoder);
    AAsset_close(asset);

    return imageData;
}

void AppXR::CreateInstance() {
    XrApplicationInfo AI;
    strncpy(AI.applicationName, "OpenXR Tutorial Chapter 3", XR_MAX_APPLICATION_NAME_SIZE);
    AI.applicationVersion = 1;
    strncpy(AI.engineName, "OpenXR Engine", XR_MAX_ENGINE_NAME_SIZE);
    AI.engineVersion = 1;
    AI.apiVersion = XR_CURRENT_API_VERSION;

    m_instanceExtensions.emplace_back(XR_EXT_DEBUG_UTILS_EXTENSION_NAME);
    m_instanceExtensions.emplace_back("XR_KHR_vulkan_enable");

    // Get all the API Layers from the OpenXR runtime.
    uint32_t apiLayerCount = 0;
    std::vector <XrApiLayerProperties> apiLayerProperties;
    OPENXR_CHECK(xrEnumerateApiLayerProperties(0, &apiLayerCount, nullptr),
                 "Failed to enumerate ApiLayerProperties.");
    apiLayerProperties.resize(apiLayerCount, {XR_TYPE_API_LAYER_PROPERTIES});
    OPENXR_CHECK(
        xrEnumerateApiLayerProperties(apiLayerCount, &apiLayerCount, apiLayerProperties.data()),
        "Failed to enumerate ApiLayerProperties.");

    // Check the requested API layers against the ones from the OpenXR. If found add it to the Active API Layers.
    for (auto &requestLayer: m_apiLayers) {
        for (auto &layerProperty: apiLayerProperties) {
            // strcmp returns 0 if the strings match.
            if (strcmp(requestLayer.c_str(), layerProperty.layerName) != 0) {
                continue;
            } else {
                m_activeAPILayers.push_back(requestLayer.c_str());
                break;
            }
        }
    }

    // Get all the Instance Extensions from the OpenXR instance.
    uint32_t extensionCount = 0;
    std::vector <XrExtensionProperties> extensionProperties;
    OPENXR_CHECK(xrEnumerateInstanceExtensionProperties(nullptr, 0, &extensionCount, nullptr),
                 "Failed to enumerate InstanceExtensionProperties.");
    extensionProperties.resize(extensionCount, {XR_TYPE_EXTENSION_PROPERTIES});
    OPENXR_CHECK(xrEnumerateInstanceExtensionProperties(nullptr, extensionCount, &extensionCount,
                                                        extensionProperties.data()),
                 "Failed to enumerate InstanceExtensionProperties.");

    // Check the requested Instance Extensions against the ones from the OpenXR runtime.
    // If an extension is found add it to Active Instance Extensions.
    // Log error if the Instance Extension is not found.
    for (auto &requestedInstanceExtension: m_instanceExtensions) {
        bool found = false;
        for (auto &extensionProperty: extensionProperties) {
            // strcmp returns 0 if the strings match.
            if (strcmp(requestedInstanceExtension.c_str(), extensionProperty.extensionName) != 0) {
                continue;
            } else {
                m_activeInstanceExtensions.push_back(requestedInstanceExtension.c_str());
                found = true;
                break;
            }
        }
        if (!found) {
            XR_TUT_LOG_ERROR("Failed to find OpenXR instance extension: " << requestedInstanceExtension);
        }
    }

    // Fill out an XrInstanceCreateInfo structure and create an XrInstance.
    XrInstanceCreateInfo instanceCI{XR_TYPE_INSTANCE_CREATE_INFO};
    instanceCI.createFlags = 0;
    instanceCI.applicationInfo = AI;
    instanceCI.enabledApiLayerCount = static_cast<uint32_t>(m_activeAPILayers.size());
    instanceCI.enabledApiLayerNames = m_activeAPILayers.data();
    instanceCI.enabledExtensionCount = static_cast<uint32_t>(m_activeInstanceExtensions.size());
    instanceCI.enabledExtensionNames = m_activeInstanceExtensions.data();
    OPENXR_CHECK(xrCreateInstance(&instanceCI, &m_xrInstance), "Failed to create Instance.");
}

void AppXR::DestroyInstance() {
    // Destroy the XrInstance.
    OPENXR_CHECK(xrDestroyInstance(m_xrInstance), "Failed to destroy Instance.");
}

void AppXR::CreateDebugMessenger() {
    // Check that "XR_EXT_debug_utils" is in the active Instance Extensions before creating an XrDebugUtilsMessengerEXT.
    if (IsStringInVector(m_activeInstanceExtensions, XR_EXT_DEBUG_UTILS_EXTENSION_NAME)) {
        m_debugUtilsMessenger = CreateOpenXRDebugUtilsMessenger(m_xrInstance);  // From OpenXRDebugUtils.h.
    }
}

void AppXR::DestroyDebugMessenger() {
    // Check that "XR_EXT_debug_utils" is in the active Instance Extensions before destroying the XrDebugUtilsMessengerEXT.
    if (m_debugUtilsMessenger != XR_NULL_HANDLE) {
        DestroyOpenXRDebugUtilsMessenger(m_xrInstance, m_debugUtilsMessenger);  // From OpenXRDebugUtils.h.
    }
}

void AppXR::GetInstanceProperties() {
    // Get the instance's properties and log the runtime name and version.
    XrInstanceProperties instanceProperties{XR_TYPE_INSTANCE_PROPERTIES};
    OPENXR_CHECK(xrGetInstanceProperties(m_xrInstance, &instanceProperties),
                 "Failed to get InstanceProperties.");

    XR_TUT_LOG("OpenXR Runtime: " << instanceProperties.runtimeName << " - "
                                  << XR_VERSION_MAJOR(instanceProperties.runtimeVersion) << "."
                                  << XR_VERSION_MINOR(instanceProperties.runtimeVersion) << "."
                                  << XR_VERSION_PATCH(instanceProperties.runtimeVersion));
}

void AppXR::GetSystemID() {
    // Get the XrSystemId from the instance and the supplied XrFormFactor.
    XrSystemGetInfo systemGI{XR_TYPE_SYSTEM_GET_INFO};
    systemGI.formFactor = m_formFactor;
    OPENXR_CHECK(xrGetSystem(m_xrInstance, &systemGI, &m_systemID), "Failed to get SystemID.");

    // Get the System's properties for some general information about the hardware and the vendor.
    OPENXR_CHECK(
        xrGetSystemProperties(m_xrInstance, m_systemID, &m_systemProperties),
         "Failed to get SystemProperties."
    );
}

void AppXR::GetEnvironmentBlendModes() {
    // Retrieves the available blend modes. The first call gets the count of the array that will be returned. The next call fills out the array.
    uint32_t environmentBlendModeCount = 0;
    OPENXR_CHECK(
        xrEnumerateEnvironmentBlendModes(m_xrInstance, m_systemID, m_viewConfiguration, 0, &environmentBlendModeCount, nullptr),
        "Failed to enumerate EnvironmentBlend Modes."
     )
    m_environmentBlendModes.resize(environmentBlendModeCount);
    OPENXR_CHECK(
        xrEnumerateEnvironmentBlendModes(
            m_xrInstance,
            m_systemID,
            m_viewConfiguration,
            environmentBlendModeCount,
            &environmentBlendModeCount,
            m_environmentBlendModes.data()
        ),
        "Failed to enumerate EnvironmentBlend Modes."
     )

    // Pick the first application supported blend mode supported by the hardware.
    for (const XrEnvironmentBlendMode &environmentBlendMode: m_applicationEnvironmentBlendModes) {
        if (std::find(m_environmentBlendModes.begin(), m_environmentBlendModes.end(),
                      environmentBlendMode) != m_environmentBlendModes.end()) {
            m_environmentBlendMode = environmentBlendMode;
            break;
        }
    }
    if (m_environmentBlendMode == XR_ENVIRONMENT_BLEND_MODE_MAX_ENUM) {
        XR_TUT_LOG_ERROR(
            "Failed to find a compatible blend mode. Defaulting to XR_ENVIRONMENT_BLEND_MODE_OPAQUE.");
        m_environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
    }
}

void AppXR::GetViewConfigurationViews() {
    // Gets the View Configuration Types. The first call gets the count of the array that will be returned. The next call fills out the array.
    uint32_t viewConfigurationCount = 0;
    OPENXR_CHECK(xrEnumerateViewConfigurations(m_xrInstance, m_systemID, 0, &viewConfigurationCount,
                                               nullptr),
                 "Failed to enumerate View Configurations.");
    m_viewConfigurations.resize(viewConfigurationCount);
    OPENXR_CHECK(xrEnumerateViewConfigurations(m_xrInstance, m_systemID, viewConfigurationCount,
                                               &viewConfigurationCount,
                                               m_viewConfigurations.data()),
                 "Failed to enumerate View Configurations.");

    // Pick the first application supported View Configuration Type con supported by the hardware.
    for (const XrViewConfigurationType &viewConfiguration: m_applicationViewConfigurations) {
        if (std::find(m_viewConfigurations.begin(), m_viewConfigurations.end(),
                      viewConfiguration) != m_viewConfigurations.end()) {
            m_viewConfiguration = viewConfiguration;
            break;
        }
    }
    if (m_viewConfiguration == XR_VIEW_CONFIGURATION_TYPE_MAX_ENUM) {
        std::cerr
            << "Failed to find a view configuration type. Defaulting to XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO."
            << std::endl;
        m_viewConfiguration = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
    }

    // Gets the View Configuration Views. The first call gets the count of the array that will be returned. The next call fills out the array.
    uint32_t viewConfigurationViewCount = 0;
    OPENXR_CHECK(xrEnumerateViewConfigurationViews(m_xrInstance, m_systemID, m_viewConfiguration, 0,
                                                   &viewConfigurationViewCount, nullptr),
                 "Failed to enumerate ViewConfiguration Views.");
    m_viewConfigurationViews.resize(viewConfigurationViewCount, {XR_TYPE_VIEW_CONFIGURATION_VIEW});
    OPENXR_CHECK(
        xrEnumerateViewConfigurationViews(
            m_xrInstance, m_systemID, m_viewConfiguration,
            viewConfigurationViewCount,
            &viewConfigurationViewCount,
            m_viewConfigurationViews.data()
        ),
        "Failed to enumerate ViewConfiguration Views."
    )
}

void AppXR::CreateSession() {
    // Create an XrSessionCreateInfo structure.
    XrSessionCreateInfo sessionCI{XR_TYPE_SESSION_CREATE_INFO};
    mpGraphicsContext_ = std::make_unique<clay::GraphicsContextXR>(m_xrInstance, m_systemID);
    mpGraphicsContextXR_ = ((GraphicsContextXR*)mpGraphicsContext_.get());
    mpResources_ = new Resources(*mpGraphicsContext_);
    InitImguiRender();

    ImGuiComponentXR::gGraphicsContext_ = mpGraphicsContext_.get();

    // Fill out the XrSessionCreateInfo structure and create an XrSession.
    sessionCI.next = ((GraphicsContextXR*)mpGraphicsContext_.get())->GetGraphicsBinding();
    sessionCI.createFlags = 0;
    sessionCI.systemId = m_systemID;

    OPENXR_CHECK(xrCreateSession(m_xrInstance, &sessionCI, &m_session), "Failed to create Session.");
}

void AppXR::DestroySession() {
    // Destroy the XrSession.
    OPENXR_CHECK(xrDestroySession(m_session), "Failed to destroy Session.");
}

void AppXR::CreateResources() {}

void AppXR::DestroyResources() {
    mScenes_.front()->destroyResources();
}

void AppXR::PollEvents() {
    // Poll OpenXR for a new event.
    XrEventDataBuffer eventData{XR_TYPE_EVENT_DATA_BUFFER};
    auto XrPollEvents = [&]() -> bool {
        eventData = {XR_TYPE_EVENT_DATA_BUFFER};
        return xrPollEvent(m_xrInstance, &eventData) == XR_SUCCESS;
    };

    while (XrPollEvents()) {
        switch (eventData.type) {
            // Log the number of lost events from the runtime.
            case XR_TYPE_EVENT_DATA_EVENTS_LOST: {
                XrEventDataEventsLost *eventsLost = reinterpret_cast<XrEventDataEventsLost *>(&eventData);
                XR_TUT_LOG("OPENXR: Events Lost: " << eventsLost->lostEventCount);
                break;
            }
                // Log that an instance loss is pending and shutdown the application.
            case XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING: {
                XrEventDataInstanceLossPending *instanceLossPending = reinterpret_cast<XrEventDataInstanceLossPending *>(&eventData);
                XR_TUT_LOG("OPENXR: Instance Loss Pending at: " << instanceLossPending->lossTime);
                m_sessionRunning = false;
                m_applicationRunning = false;
                break;
            }
                // Log that the interaction profile has changed.
            case XR_TYPE_EVENT_DATA_INTERACTION_PROFILE_CHANGED: {
                XrEventDataInteractionProfileChanged *interactionProfileChanged = reinterpret_cast<XrEventDataInteractionProfileChanged *>(&eventData);
                XR_TUT_LOG("OPENXR: Interaction Profile changed for Session: "
                               << interactionProfileChanged->session);
                if (interactionProfileChanged->session != m_session) {
                    XR_TUT_LOG("XrEventDataInteractionProfileChanged for unknown Session");
                    break;
                }
                mInputHandler_.recordCurrentBindings();
                break;
            }
                // Log that there's a reference space change pending.
            case XR_TYPE_EVENT_DATA_REFERENCE_SPACE_CHANGE_PENDING: {
                XrEventDataReferenceSpaceChangePending *referenceSpaceChangePending = reinterpret_cast<XrEventDataReferenceSpaceChangePending *>(&eventData);
                XR_TUT_LOG("OPENXR: Reference Space Change pending for Session: "
                               << referenceSpaceChangePending->session);
                if (referenceSpaceChangePending->session != m_session) {
                    XR_TUT_LOG("XrEventDataReferenceSpaceChangePending for unknown Session");
                    break;
                }
                break;
            }
                // Session State changes:
            case XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED: {
                XrEventDataSessionStateChanged *sessionStateChanged = reinterpret_cast<XrEventDataSessionStateChanged *>(&eventData);
                if (sessionStateChanged->session != m_session) {
                    XR_TUT_LOG("XrEventDataSessionStateChanged for unknown Session");
                    break;
                }

                if (sessionStateChanged->state == XR_SESSION_STATE_READY) {
                    // SessionState is ready. Begin the XrSession using the XrViewConfigurationType.
                    XrSessionBeginInfo sessionBeginInfo{XR_TYPE_SESSION_BEGIN_INFO};
                    sessionBeginInfo.primaryViewConfigurationType = m_viewConfiguration;
                    OPENXR_CHECK(xrBeginSession(m_session, &sessionBeginInfo),
                                 "Failed to begin Session.");
                    m_sessionRunning = true;
                }
                if (sessionStateChanged->state == XR_SESSION_STATE_STOPPING) {
                    // SessionState is stopping. End the XrSession.
                    OPENXR_CHECK(xrEndSession(m_session), "Failed to end Session.");
                    m_sessionRunning = false;
                }
                if (sessionStateChanged->state == XR_SESSION_STATE_EXITING) {
                    // SessionState is exiting. Exit the application.
                    m_sessionRunning = false;
                    m_applicationRunning = false;
                }
                if (sessionStateChanged->state == XR_SESSION_STATE_LOSS_PENDING) {
                    // SessionState is loss pending. Exit the application.
                    // It's possible to try a reestablish an XrInstance and XrSession, but we will simply exit here.
                    m_sessionRunning = false;
                    m_applicationRunning = false;
                }
                // Store state for reference across the application.
                m_sessionState = sessionStateChanged->state;
                break;
            }
            default: {
                break;
            }
        }
    }
}

void AppXR::CreateReferenceSpace() {
    // Fill out an XrReferenceSpaceCreateInfo structure and create a reference XrSpace, specifying a Local space with an identity pose as the origin.
    XrReferenceSpaceCreateInfo referenceSpaceCI{XR_TYPE_REFERENCE_SPACE_CREATE_INFO};
    referenceSpaceCI.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
    referenceSpaceCI.poseInReferenceSpace = {{0.0f, 0.0f, 0.0f, 1.0f},
                                             {0.0f, 0.0f, 0.0f}};
    OPENXR_CHECK(
        xrCreateReferenceSpace(m_session, &referenceSpaceCI, &m_localSpace),
        "Failed to create ReferenceSpace."
    )

    // Create the Head space (View space)
    XrReferenceSpaceCreateInfo headSpaceCI{XR_TYPE_REFERENCE_SPACE_CREATE_INFO};
    headSpaceCI.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_VIEW;
    headSpaceCI.poseInReferenceSpace = {{0.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 0.0f}};
    XrResult headSpaceResult = xrCreateReferenceSpace(m_session, &headSpaceCI, &mHeadSpace_);
    if (XR_FAILED(headSpaceResult)) {
        LOG_E("Failed to create Head space: %d", headSpaceResult);
        return;
    }
}

void AppXR::DestroyReferenceSpace() {
    // Destroy the reference XrSpace.
    OPENXR_CHECK(xrDestroySpace(m_localSpace), "Failed to destroy Space.")
    OPENXR_CHECK(xrDestroySpace(mHeadSpace_), "Failed to destroy Space.")
}

void AppXR::CreateSwapchains() {
    // Get the supported swapchain formats as an array of int64_t and ordered by runtime preference.
    uint32_t formatCount = 0;
    OPENXR_CHECK(xrEnumerateSwapchainFormats(m_session, 0, &formatCount, nullptr),
                 "Failed to enumerate Swapchain Formats");
    std::vector<int64_t> formats(formatCount);
    OPENXR_CHECK(
        xrEnumerateSwapchainFormats(m_session, formatCount, &formatCount, formats.data()),
        "Failed to enumerate Swapchain Formats"
    );
    if (mpGraphicsContextXR_->SelectDepthSwapchainFormat(formats) == 0) {
        std::cerr << "Failed to find depth format for Swapchain." << std::endl;
        DEBUG_BREAK;
    }

    //Resize the SwapchainInfo to match the number of view in the View Configuration.
    m_colorSwapchainInfos.resize(m_viewConfigurationViews.size());
    m_depthSwapchainInfos.resize(m_viewConfigurationViews.size());

    // Per view, create a color and depth swapchain, and their associated image views.
    for (size_t i = 0; i < m_viewConfigurationViews.size(); i++) {
        SwapchainInfo &colorSwapchainInfo = m_colorSwapchainInfos[i];
        SwapchainInfo &depthSwapchainInfo = m_depthSwapchainInfos[i];

        // Fill out an XrSwapchainCreateInfo structure and create an XrSwapchain.
        // Color.
        XrSwapchainCreateInfo swapchainCI{XR_TYPE_SWAPCHAIN_CREATE_INFO};
        swapchainCI.createFlags = 0;
        swapchainCI.usageFlags = XR_SWAPCHAIN_USAGE_SAMPLED_BIT | XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;
        swapchainCI.format = mpGraphicsContextXR_->SelectColorSwapchainFormat(formats); // Use GraphicsAPI to select the first compatible format.
        swapchainCI.sampleCount = m_viewConfigurationViews[i].recommendedSwapchainSampleCount;  // Use the recommended values from the XrViewConfigurationView.
        swapchainCI.width = m_viewConfigurationViews[i].recommendedImageRectWidth;
        swapchainCI.height = m_viewConfigurationViews[i].recommendedImageRectHeight;
        swapchainCI.faceCount = 1;
        swapchainCI.arraySize = 1;
        swapchainCI.mipCount = 1;
        OPENXR_CHECK(
            xrCreateSwapchain(m_session, &swapchainCI, &colorSwapchainInfo.swapchain),
            "Failed to create Color Swapchain"
         );
        colorSwapchainInfo.swapchainFormat = swapchainCI.format;  // Save the swapchain format for later use.

        // Depth.
        swapchainCI.createFlags = 0;
        swapchainCI.usageFlags = XR_SWAPCHAIN_USAGE_SAMPLED_BIT | XR_SWAPCHAIN_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        swapchainCI.format = mpGraphicsContextXR_->SelectDepthSwapchainFormat(formats);          // Use GraphicsAPI to select the first compatible format.
        swapchainCI.sampleCount = m_viewConfigurationViews[i].recommendedSwapchainSampleCount;  // Use the recommended values from the XrViewConfigurationView.
        swapchainCI.width = m_viewConfigurationViews[i].recommendedImageRectWidth;
        swapchainCI.height = m_viewConfigurationViews[i].recommendedImageRectHeight;
        swapchainCI.faceCount = 1;
        swapchainCI.arraySize = 1;
        swapchainCI.mipCount = 1;
        OPENXR_CHECK(
            xrCreateSwapchain(m_session, &swapchainCI, &depthSwapchainInfo.swapchain),
            "Failed to create Depth Swapchain"
         )
        depthSwapchainInfo.swapchainFormat = swapchainCI.format;  // Save the swapchain format for later use.

        // Get the number of images in the color/depth swapchain and allocate Swapchain image data via GraphicsAPI to store the returned array.
        uint32_t colorSwapchainImageCount = 0;
        OPENXR_CHECK(
            xrEnumerateSwapchainImages(
                colorSwapchainInfo.swapchain,
                0,
                &colorSwapchainImageCount,
                nullptr
            ),
            "Failed to enumerate Color Swapchain Images."
        );
        XrSwapchainImageBaseHeader *colorSwapchainImages = mpGraphicsContextXR_->AllocateSwapchainImageData(
            colorSwapchainInfo.swapchain, clay::GraphicsContextXR::SwapchainType::COLOR,
            colorSwapchainImageCount);
        OPENXR_CHECK(
            xrEnumerateSwapchainImages(colorSwapchainInfo.swapchain, colorSwapchainImageCount, &colorSwapchainImageCount, colorSwapchainImages),
            "Failed to enumerate Color Swapchain Images."
        )

        uint32_t depthSwapchainImageCount = 0;
        OPENXR_CHECK(
            xrEnumerateSwapchainImages(depthSwapchainInfo.swapchain, 0, &depthSwapchainImageCount, nullptr),
            "Failed to enumerate Depth Swapchain Images."
        )
        XrSwapchainImageBaseHeader *depthSwapchainImages = mpGraphicsContextXR_->AllocateSwapchainImageData(
            depthSwapchainInfo.swapchain, clay::GraphicsContextXR::SwapchainType::DEPTH,
            depthSwapchainImageCount);
        OPENXR_CHECK(
            xrEnumerateSwapchainImages(
                depthSwapchainInfo.swapchain,
                depthSwapchainImageCount,
                &depthSwapchainImageCount,
                depthSwapchainImages
            ),
            "Failed to enumerate Depth Swapchain Images."
        );

        // Per image in the swapchains, fill out a clay::GraphicsContextXR::ImageViewCreateInfo structure and create a color/depth image view.
        for (uint32_t j = 0; j < colorSwapchainImageCount; j++) {
            clay::GraphicsContextXR::ImageViewCreateInfo imageViewCI{};
            imageViewCI.image =mpGraphicsContextXR_->GetSwapchainImage(colorSwapchainInfo.swapchain, j);
            imageViewCI.type = clay::GraphicsContextXR::ImageViewCreateInfo::Type::RTV;
            imageViewCI.view = VK_IMAGE_VIEW_TYPE_2D;
            imageViewCI.format = (VkFormat) colorSwapchainInfo.swapchainFormat;
            imageViewCI.aspect = VK_IMAGE_ASPECT_COLOR_BIT;
            imageViewCI.baseMipLevel = 0;
            imageViewCI.levelCount = 1;
            imageViewCI.baseArrayLayer = 0;
            imageViewCI.layerCount = 1;
            colorSwapchainInfo.imageViews.push_back(mpGraphicsContextXR_->CreateImageView(imageViewCI));
        }
        for (uint32_t j = 0; j < depthSwapchainImageCount; j++) {
            clay::GraphicsContextXR::ImageViewCreateInfo imageViewCI{};
            imageViewCI.image = mpGraphicsContextXR_->GetSwapchainImage(depthSwapchainInfo.swapchain, j);
            imageViewCI.type = clay::GraphicsContextXR::ImageViewCreateInfo::Type::DSV;
            imageViewCI.view = VK_IMAGE_VIEW_TYPE_2D;
            imageViewCI.format = (VkFormat) depthSwapchainInfo.swapchainFormat;
            imageViewCI.aspect = VK_IMAGE_ASPECT_DEPTH_BIT;
            imageViewCI.baseMipLevel = 0;
            imageViewCI.levelCount = 1;
            imageViewCI.baseArrayLayer = 0;
            imageViewCI.layerCount = 1;
            depthSwapchainInfo.imageViews.push_back(mpGraphicsContextXR_->CreateImageView(imageViewCI));
        }
    }
}

void AppXR::DestroySwapchains() {
    // Per view in the view configuration:
    for (size_t i = 0; i < m_viewConfigurationViews.size(); i++) {
        SwapchainInfo &colorSwapchainInfo = m_colorSwapchainInfos[i];
        SwapchainInfo &depthSwapchainInfo = m_depthSwapchainInfos[i];

        // Destroy the color and depth image views from GraphicsAPI.
        for (auto &imageView: colorSwapchainInfo.imageViews) {
            mpGraphicsContextXR_->DestroyImageView(imageView);
        }
        for (auto &imageView: depthSwapchainInfo.imageViews) {
            mpGraphicsContextXR_->DestroyImageView(imageView);
        }

        // Free the Swapchain Image Data.
        mpGraphicsContextXR_->FreeSwapchainImageData(colorSwapchainInfo.swapchain);
        mpGraphicsContextXR_->FreeSwapchainImageData(depthSwapchainInfo.swapchain);

        // Destroy the swapchains.
        OPENXR_CHECK(xrDestroySwapchain(colorSwapchainInfo.swapchain), "Failed to destroy Color Swapchain");
        OPENXR_CHECK(xrDestroySwapchain(depthSwapchainInfo.swapchain), "Failed to destroy Depth Swapchain");
    }
}

void AppXR::RenderFrame() {
    // Get the XrFrameState for timing and rendering info.
    XrFrameState frameState{XR_TYPE_FRAME_STATE};
    XrFrameWaitInfo frameWaitInfo{XR_TYPE_FRAME_WAIT_INFO};
    OPENXR_CHECK(xrWaitFrame(m_session, &frameWaitInfo, &frameState), "Failed to wait for XR Frame.");

    // Tell the OpenXR compositor that the application is beginning the frame.
    XrFrameBeginInfo frameBeginInfo{XR_TYPE_FRAME_BEGIN_INFO};
    OPENXR_CHECK(xrBeginFrame(m_session, &frameBeginInfo), "Failed to begin the XR Frame.");

    // Variables for rendering and layer composition.
    bool rendered;
    RenderLayerInfo renderLayerInfo;
    renderLayerInfo.predictedDisplayTime = frameState.predictedDisplayTime;

    // Check that the session is active and that we should render.
    bool sessionActive = (m_sessionState == XR_SESSION_STATE_SYNCHRONIZED ||
                          m_sessionState == XR_SESSION_STATE_VISIBLE ||
                          m_sessionState == XR_SESSION_STATE_FOCUSED);
    if (sessionActive && frameState.shouldRender) {
        mInputHandler_.pollActions(frameState.predictedDisplayTime);
        // Render the stereo image and associate one of swapchain images with the XrCompositionLayerProjection structure.
        rendered = RenderLayer(renderLayerInfo);
        if (rendered) {
            renderLayerInfo.layers.push_back(reinterpret_cast<XrCompositionLayerBaseHeader *>(&renderLayerInfo.layerProjection));
        }
    }

    // Tell OpenXR that we are finished with this frame; specifying its display time, environment blending and layers.
    XrFrameEndInfo frameEndInfo{XR_TYPE_FRAME_END_INFO};
    frameEndInfo.displayTime = frameState.predictedDisplayTime;
    frameEndInfo.environmentBlendMode = m_environmentBlendMode;
    frameEndInfo.layerCount = static_cast<uint32_t>(renderLayerInfo.layers.size());
    frameEndInfo.layers = renderLayerInfo.layers.data();
    OPENXR_CHECK(xrEndFrame(m_session, &frameEndInfo), "Failed to end the XR Frame.")
}

bool AppXR::RenderLayer(RenderLayerInfo &renderLayerInfo) {
    // Locate the views from the view configuration within the (reference) space at the display time.
    std::vector <XrView> views(m_viewConfigurationViews.size(), {XR_TYPE_VIEW});

    XrViewState viewState{XR_TYPE_VIEW_STATE};  // Will contain information on whether the position and/or orientation is valid and/or tracked.
    XrViewLocateInfo viewLocateInfo{XR_TYPE_VIEW_LOCATE_INFO};
    viewLocateInfo.viewConfigurationType = m_viewConfiguration;
    viewLocateInfo.displayTime = renderLayerInfo.predictedDisplayTime;
    viewLocateInfo.space = m_localSpace;
    uint32_t viewCount = 0;
    XrResult result = xrLocateViews(
        m_session,
        &viewLocateInfo,
        &viewState,
        static_cast<uint32_t>(views.size()), &viewCount, views.data()
    );
    if (result != XR_SUCCESS) {
        XR_TUT_LOG("Failed to locate Views.");
        return false;
    }

    // Resize the layer projection views to match the view count. The layer projection views are used in the layer projection.
    renderLayerInfo.layerProjectionViews.resize(
        viewCount,
        {XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW}
    );

    // TODO use real time
    if (mScenes_.size() > 1) {
        mScenes_.erase(mScenes_.begin(), std::prev(mScenes_.end()));
        mScenes_.front()->initialize();
    }
    mScenes_.front()->update(0);
    // draw imgui onto a different frame buffer

    vkWaitForFences(mpGraphicsContextXR_->getDevice(), 1, &mpGraphicsContextXR_->fence, true, UINT64_MAX);
    if (vkResetFences(mpGraphicsContextXR_->getDevice(), 1, &mpGraphicsContextXR_->fence) != VK_SUCCESS) {
        throw std::runtime_error("Failed to reset fence");
    }

    vkResetCommandBuffer(imguiCommandBuffer, VkCommandBufferResetFlagBits(0));
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    if (vkBeginCommandBuffer(imguiCommandBuffer, &beginInfo) != VK_SUCCESS) {
        throw std::runtime_error("failed to begin recording command buffer!");
    }

    VkRenderPassBeginInfo renderPassBegin {};
    renderPassBegin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassBegin.pNext = nullptr;
    renderPassBegin.renderPass = mpGraphicsContextXR_->imguiRenderPass;
    renderPassBegin.framebuffer = imguiFrameBuffer;
    renderPassBegin.renderArea.offset = {0, 0};
    renderPassBegin.renderArea.extent.width = imguiWidth;
    renderPassBegin.renderArea.extent.height = imguiHeight;
    renderPassBegin.clearValueCount = 1;
    VkClearValue clearColor = {0.0f,0.0f,0.0f,1.0f};
    renderPassBegin.pClearValues = &clearColor;
    vkCmdBeginRenderPass(imguiCommandBuffer, &renderPassBegin, VK_SUBPASS_CONTENTS_INLINE);

    mScenes_.front()->renderGUI(imguiCommandBuffer);

    vkCmdEndRenderPass(imguiCommandBuffer);
    if (vkEndCommandBuffer(imguiCommandBuffer) != VK_SUCCESS) {
        throw std::runtime_error("failed to record command buffer!");
    }

    VkPipelineStageFlags waitDstStageMask = VkPipelineStageFlagBits::VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

    VkSubmitInfo submitInfo{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.pNext = nullptr;
    submitInfo.waitSemaphoreCount = mpGraphicsContextXR_->acquireSemaphore ? 1 : 0;
    submitInfo.pWaitSemaphores = mpGraphicsContextXR_->acquireSemaphore ? &mpGraphicsContextXR_->acquireSemaphore : nullptr;
    submitInfo.pWaitDstStageMask = mpGraphicsContextXR_->acquireSemaphore ? &waitDstStageMask : nullptr;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &imguiCommandBuffer;
    submitInfo.signalSemaphoreCount = mpGraphicsContextXR_->submitSemaphore ? 1 : 0;
    submitInfo.pSignalSemaphores = mpGraphicsContextXR_->submitSemaphore ? &mpGraphicsContextXR_->submitSemaphore : nullptr;

    vkQueueSubmit(mpGraphicsContextXR_->mQueue_, 1, &submitInfo, mpGraphicsContextXR_->fence);

    // imgui end

    // Per view in the view configuration:
    for (uint32_t i = 0; i < viewCount; i++) {
        SwapchainInfo &colorSwapchainInfo = m_colorSwapchainInfos[i];
        SwapchainInfo &depthSwapchainInfo = m_depthSwapchainInfos[i];

        // Acquire and wait for an image from the swapchains.
        // Get the image index of an image in the swapchains.
        // The timeout is infinite.
        uint32_t colorImageIndex = 0;
        uint32_t depthImageIndex = 0;
        XrSwapchainImageAcquireInfo acquireInfo{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
        OPENXR_CHECK(
            xrAcquireSwapchainImage(colorSwapchainInfo.swapchain, &acquireInfo, &colorImageIndex),
            "Failed to acquire Image from the Color Swapchian"
        )
        OPENXR_CHECK(
            xrAcquireSwapchainImage(depthSwapchainInfo.swapchain, &acquireInfo, &depthImageIndex),
            "Failed to acquire Image from the Depth Swapchian"
        )

        XrSwapchainImageWaitInfo waitInfo = {XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};
        waitInfo.timeout = XR_INFINITE_DURATION;
        OPENXR_CHECK(xrWaitSwapchainImage(colorSwapchainInfo.swapchain, &waitInfo),
                     "Failed to wait for Image from the Color Swapchain");
        OPENXR_CHECK(xrWaitSwapchainImage(depthSwapchainInfo.swapchain, &waitInfo),
                     "Failed to wait for Image from the Depth Swapchain");

        // Get the width and height and construct the viewport and scissors.
        const uint32_t &width = m_viewConfigurationViews[i].recommendedImageRectWidth;
        const uint32_t &height = m_viewConfigurationViews[i].recommendedImageRectHeight;
        clay::GraphicsContextXR::Viewport viewport = {0.0f, 0.0f, (float) width, (float) height, 0.0f, 1.0f};
        clay::GraphicsContextXR::Rect2D scissor = {{(int32_t) 0, (int32_t) 0},
                                                   {width,       height}};
        float nearZ = 0.05f;
        float farZ = 100.0f;

        // Fill out the XrCompositionLayerProjectionView structure specifying the pose and fov from the view.
        // This also associates the swapchain image with this layer projection view.
        renderLayerInfo.layerProjectionViews[i] = {XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW};
        renderLayerInfo.layerProjectionViews[i].pose = views[i].pose;
        renderLayerInfo.layerProjectionViews[i].fov = views[i].fov;
        renderLayerInfo.layerProjectionViews[i].subImage.swapchain = colorSwapchainInfo.swapchain;
        renderLayerInfo.layerProjectionViews[i].subImage.imageRect.offset.x = 0;
        renderLayerInfo.layerProjectionViews[i].subImage.imageRect.offset.y = 0;
        renderLayerInfo.layerProjectionViews[i].subImage.imageRect.extent.width = static_cast<int32_t>(width);
        renderLayerInfo.layerProjectionViews[i].subImage.imageRect.extent.height = static_cast<int32_t>(height);
        renderLayerInfo.layerProjectionViews[i].subImage.imageArrayIndex = 0;  // Useful for multiview rendering.

        // Rendering code to clear the color and depth image views.
        mpGraphicsContextXR_->BeginRendering();

        if (m_environmentBlendMode == XR_ENVIRONMENT_BLEND_MODE_OPAQUE) {
            // VR mode use a background color.
            mpGraphicsContextXR_->ClearColor(colorSwapchainInfo.imageViews[colorImageIndex], 0.17f, 0.17f,
                                      0.17f, 1.00f);
        } else {
            // In AR mode make the background color black.
            mpGraphicsContextXR_->ClearColor(colorSwapchainInfo.imageViews[colorImageIndex], 0.00f, 0.00f, 0.00f, 1.00f);
        }
        mpGraphicsContextXR_->ClearDepth(depthSwapchainInfo.imageViews[depthImageIndex], 1.0f);

        mpGraphicsContextXR_->SetRenderAttachments(
            &colorSwapchainInfo.imageViews[colorImageIndex], 1,
            depthSwapchainInfo.imageViews[depthImageIndex], width,
            height
        );
        mpGraphicsContextXR_->SetViewports(&viewport, 1);
        mpGraphicsContextXR_->SetScissors(&scissor, 1);

        // Compute the view-projection transform.
        const Camera* pCamera = mScenes_.front()->getFocusCamera();
        const glm::mat4 glmProj = utils::computeProjectionMatrix(views[i].fov, pCamera->getNear(), pCamera->getFar());

        const glm::mat4 glmViewWorldLocked = utils::computeWorldLockViewMatrix(
            views[i].pose,
            pCamera->getPosition(),
            pCamera->getOrientation(),
            mInputHandler_.getHeadPose()
        );

        // world locked
        mScenes_.front()->cameraConstants.view = glmViewWorldLocked;
        mScenes_.front()->cameraConstants.proj = glmProj;

        const glm::mat4 glmViewHeadLocked = utils::computeHeadLockViewMatrix(views[i].pose);

        // head locked
        mScenes_.front()->cameraConstants2.view = glmViewHeadLocked;
        mScenes_.front()->cameraConstants2.proj = glmProj;

        mScenes_.front()->render(mpGraphicsContextXR_->cmdBuffer);

        mpGraphicsContextXR_->EndRendering();
        // Give the swapchain image back to OpenXR, allowing the compositor to use the image.
        XrSwapchainImageReleaseInfo releaseInfo{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
        OPENXR_CHECK(xrReleaseSwapchainImage(colorSwapchainInfo.swapchain, &releaseInfo),
                     "Failed to release Image back to the Color Swapchain");
        OPENXR_CHECK(xrReleaseSwapchainImage(depthSwapchainInfo.swapchain, &releaseInfo),
                     "Failed to release Image back to the Depth Swapchain");
    }

    // Fill out the XrCompositionLayerProjection structure for usage with xrEndFrame().
    renderLayerInfo.layerProjection.layerFlags =
        XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT |
        XR_COMPOSITION_LAYER_CORRECT_CHROMATIC_ABERRATION_BIT;
    renderLayerInfo.layerProjection.space = m_localSpace;
    renderLayerInfo.layerProjection.viewCount = static_cast<uint32_t>(renderLayerInfo.layerProjectionViews.size());
    renderLayerInfo.layerProjection.views = renderLayerInfo.layerProjectionViews.data();

    return true;
}

void AppXR::PollSystemEvents() {
    // Checks whether Android has requested that application should by destroyed.
    if (mpAndroidApp_->destroyRequested != 0) {
        m_applicationRunning = false;
        return;
    }
    while (true) {
        // Poll and process the Android OS system events.
        struct android_poll_source* source = nullptr;
        int events = 0;
        // The timeout depends on whether the application is active.
        const int timeoutMilliseconds = (!mAndroidAppState_.resumed && !m_sessionRunning && mpAndroidApp_->destroyRequested == 0) ? -1 : 0;
        if (ALooper_pollOnce(timeoutMilliseconds, nullptr, &events, (void **) &source) >= 0) {
            if (source != nullptr) {
                // this calls mpAndroidApp_->onAppCmd
                source->process(mpAndroidApp_, source);
            }
        } else {
            break;
        }
    }
}

void AppXR::AndroidAppHandleCmd(struct android_app* app, int32_t cmd) {
    auto* appState = (AndroidAppState*) app->userData;

    switch (cmd) {
        // There is no APP_CMD_CREATE. The ANativeActivity creates the application thread from onCreate().
        // The application thread then calls android_main().
        case APP_CMD_START: {
            break;
        }
        case APP_CMD_RESUME: {
            appState->resumed = true;
            break;
        }
        case APP_CMD_PAUSE: {
            appState->resumed = false;
            break;
        }
        case APP_CMD_STOP: {
            break;
        }
        case APP_CMD_DESTROY: {
            appState->nativeWindow = nullptr;
            break;
        }
        case APP_CMD_INIT_WINDOW: {
            appState->nativeWindow = app->window;
            appState->windowInitialized = true;
            break;
        }
        case APP_CMD_TERM_WINDOW: {
            appState->nativeWindow = nullptr;
            break;
        }
    }
}

void AppXR::InitImguiRender() {
    // command buffer

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = mpGraphicsContext_->mCommandPool_;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;

    if (vkAllocateCommandBuffers(mpGraphicsContext_->getDevice(), &allocInfo, &imguiCommandBuffer) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate command buffers!");
    }

    // create renderpass
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = VK_FORMAT_R8G8B8A8_SRGB;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0;  // Index in pAttachments array
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;

    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments = &colorAttachment;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;

    if (vkCreateRenderPass(mpGraphicsContext_->getDevice(), &renderPassInfo, nullptr, &mpGraphicsContextXR_->imguiRenderPass) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create render pass");
    }

    mpGraphicsContext_->createImage(
        imguiWidth,
        imguiHeight,
        1,
        VK_SAMPLE_COUNT_1_BIT,
        VK_FORMAT_R8G8B8A8_SRGB,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, // VK_SHARING_MODE_EXCLUSIVE?
        imguiImage,
        imguiImageMemory
    );

    // TODO confirm this is needed. I added this before i realized i had a mistake in my camera buffer setting for imgui
    mpGraphicsContextXR_->transitionImageLayout(
        imguiImage,
        VK_FORMAT_R8G8B8A8_SRGB,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        1
    );

    imguiImageView = mpGraphicsContext_->createImageView(
        imguiImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT, 1
    );

    VkImageView attachments[] = {
        imguiImageView
    };

    VkFramebufferCreateInfo framebufferInfo{};
    framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebufferInfo.renderPass = mpGraphicsContextXR_->imguiRenderPass;
    framebufferInfo.attachmentCount = 1;
    framebufferInfo.pAttachments = attachments;
    framebufferInfo.width = imguiWidth;
    framebufferInfo.height = imguiHeight;
    framebufferInfo.layers = 1;  // usually 1 unless rendering to a 3D image or array layers

    if (vkCreateFramebuffer(mpGraphicsContext_->getDevice(), &framebufferInfo, nullptr, &imguiFrameBuffer) != VK_SUCCESS) {
        throw std::runtime_error("failed to create framebuffer!");
    }
}

} // namespace clay

#endif // CLAY_PLATFORM_XR