#ifdef CLAY_PLATFORM_XR
#include "clay/application/xr/XRSystem.h"

namespace clay {

//static int32_t handleInputEvent(struct android_app* /*app*/, AInputEvent* inputEvent) {
//    //return ImGui_ImplAndroid_HandleInputEvent(inputEvent);
//}

void XRSystem::AndroidAppHandleCmd(struct android_app *app, int32_t cmd) {
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

XRSystem::XRSystem(android_app* pAndroidApp) {
    mpAndroidApp_ = pAndroidApp;
    mpAndroidApp_->userData = &mAndroidAppState_;
    mpAndroidApp_->onAppCmd = XRSystem::AndroidAppHandleCmd;
}

void XRSystem::initialize() {
    CreateInstance();
    CreateDebugMessenger();
    GetInstanceProperties();
    GetSystemID();
    GetViewConfigurationViews();
    GetEnvironmentBlendModes();
    CreateSession();
    CreateReferenceSpace();
    CreateSwapchains();
    mpGraphicsContext_->CreateRenderPass(
        {m_colorSwapchainInfos[0].swapchainFormat},
        m_depthSwapchainInfos[0].swapchainFormat
    );

    while (!mAndroidAppState_.windowInitialized && m_applicationRunning) {
        PollSystemEvents(); // allow state to update
        PollEvents();
    }
}

void XRSystem::finalize() {
    DestroySwapchains();
    DestroyReferenceSpace();
    DestroyResources();
    DestroySession();
    DestroyDebugMessenger();
    DestroyInstance();
}

void XRSystem::PollSystemEvents() {
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

void XRSystem::CreateInstance() {
    XrApplicationInfo AI;
    strncpy(AI.applicationName, "ClayEngineVK XR Demo", XR_MAX_APPLICATION_NAME_SIZE);
    AI.applicationVersion = 1;
    strncpy(AI.engineName, "ClayEngineVK XR Demo", XR_MAX_ENGINE_NAME_SIZE);
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

void XRSystem::DestroyInstance() {
    OPENXR_CHECK(xrDestroyInstance(m_xrInstance), "Failed to destroy Instance.");
}

void XRSystem::CreateDebugMessenger() {
    // Check that "XR_EXT_debug_utils" is in the active Instance Extensions before creating an XrDebugUtilsMessengerEXT.
    if (IsStringInVector(m_activeInstanceExtensions, XR_EXT_DEBUG_UTILS_EXTENSION_NAME)) {
        m_debugUtilsMessenger = CreateOpenXRDebugUtilsMessenger(m_xrInstance);  // From OpenXRDebugUtils.h.
    }
}

void XRSystem::DestroyDebugMessenger() {
    // Check that "XR_EXT_debug_utils" is in the active Instance Extensions before destroying the XrDebugUtilsMessengerEXT.
    if (m_debugUtilsMessenger != XR_NULL_HANDLE) {
        DestroyOpenXRDebugUtilsMessenger(m_xrInstance, m_debugUtilsMessenger);  // From OpenXRDebugUtils.h.
    }
}

void XRSystem::GetInstanceProperties() {
    // Get the instance's properties and log the runtime name and version.
    XrInstanceProperties instanceProperties{XR_TYPE_INSTANCE_PROPERTIES};
    OPENXR_CHECK(xrGetInstanceProperties(m_xrInstance, &instanceProperties),
        "Failed to get InstanceProperties.");

    XR_TUT_LOG("OpenXR Runtime: " << instanceProperties.runtimeName << " - "
                                  << XR_VERSION_MAJOR(instanceProperties.runtimeVersion) << "."
                                  << XR_VERSION_MINOR(instanceProperties.runtimeVersion) << "."
                                  << XR_VERSION_PATCH(instanceProperties.runtimeVersion));
}

void XRSystem::GetSystemID() {
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

void XRSystem::GetEnvironmentBlendModes() {
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

void XRSystem::GetViewConfigurationViews() {
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

void XRSystem::CreateSession() {
    // Create an XrSessionCreateInfo structure.
    XrSessionCreateInfo sessionCI{XR_TYPE_SESSION_CREATE_INFO};

    mpGraphicsContext_ = new GraphicsContextXR(m_xrInstance, m_systemID);
    // Fill out the XrSessionCreateInfo structure and create an XrSession.
    sessionCI.next = mpGraphicsContext_->GetGraphicsBinding();
    sessionCI.createFlags = 0;
    sessionCI.systemId = m_systemID;

    OPENXR_CHECK(xrCreateSession(m_xrInstance, &sessionCI, &m_session), "Failed to create Session.");
}

void XRSystem::DestroySession() {
    OPENXR_CHECK(xrDestroySession(m_session), "Failed to destroy Session.");
}

void XRSystem::DestroyResources() {

}

void XRSystem::PollEvents() {
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
                // mInputHandler_.recordCurrentBindings(); // TODO fix this
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

void XRSystem::CreateReferenceSpace() {
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
        // LOG_E("Failed to create Head space: %d", headSpaceResult);
        return;
    }
}

void XRSystem::DestroyReferenceSpace() {
    OPENXR_CHECK(xrDestroySpace(m_localSpace), "Failed to destroy Space.")
    OPENXR_CHECK(xrDestroySpace(mHeadSpace_), "Failed to destroy Space.")
}

void XRSystem::CreateSwapchains() {
    // Get the supported swapchain formats as an array of int64_t and ordered by runtime preference.
    uint32_t formatCount = 0;
    OPENXR_CHECK(xrEnumerateSwapchainFormats(m_session, 0, &formatCount, nullptr),
        "Failed to enumerate Swapchain Formats");
    std::vector<int64_t> formats(formatCount);
    OPENXR_CHECK(
        xrEnumerateSwapchainFormats(m_session, formatCount, &formatCount, formats.data()),
        "Failed to enumerate Swapchain Formats"
    );
    if (mpGraphicsContext_->SelectDepthSwapchainFormat(formats) == 0) {
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
        swapchainCI.usageFlags = XR_SWAPCHAIN_USAGE_SAMPLED_BIT | XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT;
        swapchainCI.format = mpGraphicsContext_->SelectColorSwapchainFormat(formats); // Use GraphicsAPI to select the first compatible format.
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
        swapchainCI.usageFlags = XR_SWAPCHAIN_USAGE_SAMPLED_BIT | XR_SWAPCHAIN_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT;
        swapchainCI.format = mpGraphicsContext_->SelectDepthSwapchainFormat(formats);          // Use GraphicsAPI to select the first compatible format.
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
        XrSwapchainImageBaseHeader *colorSwapchainImages = mpGraphicsContext_->AllocateSwapchainImageData(
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
        XrSwapchainImageBaseHeader *depthSwapchainImages = mpGraphicsContext_->AllocateSwapchainImageData(
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
            imageViewCI.image = mpGraphicsContext_->GetSwapchainImage(colorSwapchainInfo.swapchain, j);
            imageViewCI.type = clay::GraphicsContextXR::ImageViewCreateInfo::Type::RTV;
            imageViewCI.view = VK_IMAGE_VIEW_TYPE_2D;
            imageViewCI.format = (VkFormat) colorSwapchainInfo.swapchainFormat;
            imageViewCI.aspect = VK_IMAGE_ASPECT_COLOR_BIT;
            imageViewCI.baseMipLevel = 0;
            imageViewCI.levelCount = 1;
            imageViewCI.baseArrayLayer = 0;
            imageViewCI.layerCount = 1;
            colorSwapchainInfo.imageViews.push_back(mpGraphicsContext_->CreateImageView(imageViewCI));
        }
        for (uint32_t j = 0; j < depthSwapchainImageCount; j++) {
            clay::GraphicsContextXR::ImageViewCreateInfo imageViewCI{};
            imageViewCI.image = mpGraphicsContext_->GetSwapchainImage(depthSwapchainInfo.swapchain, j);
            imageViewCI.type = clay::GraphicsContextXR::ImageViewCreateInfo::Type::DSV;
            imageViewCI.view = VK_IMAGE_VIEW_TYPE_2D;
            imageViewCI.format = (VkFormat) depthSwapchainInfo.swapchainFormat;
            imageViewCI.aspect = VK_IMAGE_ASPECT_DEPTH_BIT;
            imageViewCI.baseMipLevel = 0;
            imageViewCI.levelCount = 1;
            imageViewCI.baseArrayLayer = 0;
            imageViewCI.layerCount = 1;
            depthSwapchainInfo.imageViews.push_back(mpGraphicsContext_->CreateImageView(imageViewCI));
        }
    }
}

void XRSystem::DestroySwapchains() {
    // Per view in the view configuration:
    for (size_t i = 0; i < m_viewConfigurationViews.size(); i++) {
        SwapchainInfo &colorSwapchainInfo = m_colorSwapchainInfos[i];
        SwapchainInfo &depthSwapchainInfo = m_depthSwapchainInfos[i];

        // Destroy the color and depth image views from GraphicsAPI.
        for (auto &imageView: colorSwapchainInfo.imageViews) {
            mpGraphicsContext_->DestroyImageView(imageView);
        }
        for (auto &imageView: depthSwapchainInfo.imageViews) {
            mpGraphicsContext_->DestroyImageView(imageView);
        }

        // Free the Swapchain Image Data.
        mpGraphicsContext_->FreeSwapchainImageData(colorSwapchainInfo.swapchain);
        mpGraphicsContext_->FreeSwapchainImageData(depthSwapchainInfo.swapchain);

        // Destroy the swapchains.
        OPENXR_CHECK(xrDestroySwapchain(colorSwapchainInfo.swapchain), "Failed to destroy Color Swapchain");
        OPENXR_CHECK(xrDestroySwapchain(depthSwapchainInfo.swapchain), "Failed to destroy Depth Swapchain");
    }
}

} // namespace clay

#endif // CLAY_PLATFORM_XR