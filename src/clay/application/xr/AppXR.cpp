#ifdef CLAY_PLATFORM_XR

//clay
#include "clay/utils/common/Logger.h"
#include "clay/gui/xr/ImGuiComponentXR.h"
#include "clay/gui/xr/UiComponentXR.h"
#include <algorithm>
#include <chrono>
#include <array>
#include <cmath>
// class
#include "clay/application/xr/AppXR.h"
// third party
#include <android/asset_manager.h>
#include <android/bitmap.h>
#include <android/imagedecoder.h>

namespace clay {

AppXR::AppXR(XRSystem* xrSystem)
    : BaseApp(xrSystem->mpGraphicsContext_) {
    mXRSystem_ = xrSystem;
}

void AppXR::initialize() {}

void AppXR::Run() {
    mInputHandler_.initialize(
        mXRSystem_->m_xrInstance, mXRSystem_->m_session, mXRSystem_->m_localSpace, mXRSystem_->mHeadSpace_
    );

    InitImguiRender();
    ImGuiComponentXR::gGraphicsContext_ = mpGraphicsContext_.get();
    ImGuiComponentXR::initialize(mXRSystem_->mAndroidAppState_.nativeWindow);
    CreateResources();

    bool wasActive = false;
    while (mXRSystem_->m_applicationRunning) {
        mXRSystem_->PollSystemEvents();
        mXRSystem_->PollEvents();
        // Render only while the session runs AND the activity is resumed. While the activity
        // is paused the runtime makes xrWaitFrame return at once, so rendering then would
        // spin the CPU/GPU flat out for frames nobody sees.
        const bool active = mXRSystem_->m_sessionRunning && mXRSystem_->mAndroidAppState_.resumed;
        if (active != wasActive) {
            wasActive = active;
            onSessionRunningChanged(active);
        }
        if (active) {
            RenderFrame();
        }
    }
}

AAssetManager* AppXR::getAssetManager() {
    return mXRSystem_->mpAndroidApp_->activity->assetManager;
}

void AppXR::setScene(BaseScene* newScene) {
    mScenes_.emplace_back(newScene);
}

InputHandlerXR& AppXR::getInputHandler() {
    return mInputHandler_;
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

void AppXR::CreateResources() {}

void AppXR::RenderFrame() {
    // Get the XrFrameState for timing and rendering info.
    XrFrameState frameState{XR_TYPE_FRAME_STATE};
    XrFrameWaitInfo frameWaitInfo{XR_TYPE_FRAME_WAIT_INFO};
    xrWaitFrame(mXRSystem_->m_session, &frameWaitInfo, &frameState);

    // Tell the OpenXR compositor that the application is beginning the frame.
    XrFrameBeginInfo frameBeginInfo{XR_TYPE_FRAME_BEGIN_INFO};
    xrBeginFrame(mXRSystem_->m_session, &frameBeginInfo);

    // Variables for rendering and layer composition.
    bool rendered;
    XRSystem::RenderLayerInfo renderLayerInfo;
    renderLayerInfo.predictedDisplayTime = frameState.predictedDisplayTime;

    // Check that the session is active and that we should render.
    bool sessionActive = (mXRSystem_->m_sessionState == XR_SESSION_STATE_SYNCHRONIZED ||
                          mXRSystem_->m_sessionState == XR_SESSION_STATE_VISIBLE ||
                          mXRSystem_->m_sessionState == XR_SESSION_STATE_FOCUSED);
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
    frameEndInfo.environmentBlendMode = mXRSystem_->m_environmentBlendMode;
    frameEndInfo.layerCount = static_cast<uint32_t>(renderLayerInfo.layers.size());
    frameEndInfo.layers = renderLayerInfo.layers.data();
    xrEndFrame(mXRSystem_->m_session, &frameEndInfo);
}

bool AppXR::RenderLayer(XRSystem::RenderLayerInfo &renderLayerInfo) {
    // Locate the views from the view configuration within the (reference) space at the display time.
    std::vector <XrView> views(mXRSystem_->m_viewConfigurationViews.size(), {XR_TYPE_VIEW});

    XrViewState viewState{XR_TYPE_VIEW_STATE};  // Will contain information on whether the position and/or orientation is valid and/or tracked.
    XrViewLocateInfo viewLocateInfo{XR_TYPE_VIEW_LOCATE_INFO};
    viewLocateInfo.viewConfigurationType = mXRSystem_->m_viewConfiguration;
    viewLocateInfo.displayTime = renderLayerInfo.predictedDisplayTime;
    viewLocateInfo.space = mXRSystem_->m_localSpace;
    uint32_t viewCount = 0;
    XrResult result = xrLocateViews(
        mXRSystem_->m_session,
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

    GraphicsContextXR& gfx = *mXRSystem_->mpGraphicsContext_;

    // One command buffer and one submit per frame. BeginRendering() waits for the previous
    // frame's GPU work - the only CPU wait in the frame - so everything below (scene
    // switches, scene update writing uniforms/bone palettes, the camera uniforms) can safely
    // rewrite data the GPU reads. The GPU then runs this frame while the CPU goes on to
    // xrEndFrame / xrWaitFrame of the next one.
    gfx.BeginRendering();
    vk::CommandBuffer cmd = gfx.cmdBuffer;

    if (mScenes_.size() > 1) {
        mScenes_.erase(mScenes_.begin(), std::prev(mScenes_.end()));
        mScenes_.front()->initialize();
    }
    // Real frame time (seconds, capped so a stall or a resume isn't one giant step).
    float dt = 0.0f;
    {
        const auto now = std::chrono::steady_clock::now();
        if (mLastFrameTime_.time_since_epoch().count() != 0) {
            dt = std::min(std::chrono::duration<float>(now - mLastFrameTime_).count(), 0.1f);
        }
        mLastFrameTime_ = now;
    }
    mScenes_.front()->update(dt);

    // --- GUI panels: each drawn into its offscreen target (mips rebuilt), sampled below ---
    if (mImguiPanelEnabled_) {
        mImguiTarget_.begin(cmd);
        mScenes_.front()->renderGUI(cmd);
        mImguiTarget_.end(cmd);
    }

    UiComponentXR::render(cmd); // the RmlUi panel, if one is loaded

    // --- Offscreen passes the scene needs before the main pass (e.g. a shadow map) ---------
    mScenes_.front()->renderShadowPass(cmd);

    // --- Scene: both eyes in one multiview pass -------------------------------------------
    // One layered swapchain (layer i = view i); see XRSystem::CreateSwapchains.
    XRSystem::SwapchainInfo &colorSwapchainInfo = mXRSystem_->m_colorSwapchainInfos[0];
    XRSystem::SwapchainInfo &depthSwapchainInfo = mXRSystem_->m_depthSwapchainInfos[0];

    uint32_t colorImageIndex = 0;
    uint32_t depthImageIndex = 0;
    XrSwapchainImageAcquireInfo acquireInfo{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
    xrAcquireSwapchainImage(colorSwapchainInfo.swapchain, &acquireInfo, &colorImageIndex);
    xrAcquireSwapchainImage(depthSwapchainInfo.swapchain, &acquireInfo, &depthImageIndex);

    XrSwapchainImageWaitInfo waitInfo = {XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};
    waitInfo.timeout = XR_INFINITE_DURATION;
    xrWaitSwapchainImage(colorSwapchainInfo.swapchain, &waitInfo);
    xrWaitSwapchainImage(depthSwapchainInfo.swapchain, &waitInfo);

    const uint32_t width = mXRSystem_->mEyeWidth_;
    const uint32_t height = mXRSystem_->mEyeHeight_;

    // Per-eye camera matrices, all written once; shaders pick theirs with gl_ViewIndex.
    const Camera* pCamera = mScenes_.front()->getFocusCamera();
    GraphicsContextXR::StereoCamera worldLockedUniform{};
    GraphicsContextXR::StereoCamera headLockedUniform{};
    const uint32_t stereoViews = std::min<uint32_t>(viewCount, 2);
    for (uint32_t i = 0; i < stereoViews; i++) {
        renderLayerInfo.layerProjectionViews[i] = {XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW};
        renderLayerInfo.layerProjectionViews[i].pose = views[i].pose;
        renderLayerInfo.layerProjectionViews[i].fov = views[i].fov;
        renderLayerInfo.layerProjectionViews[i].subImage.swapchain = colorSwapchainInfo.swapchain;
        renderLayerInfo.layerProjectionViews[i].subImage.imageRect.offset.x = 0;
        renderLayerInfo.layerProjectionViews[i].subImage.imageRect.offset.y = 0;
        renderLayerInfo.layerProjectionViews[i].subImage.imageRect.extent.width = static_cast<int32_t>(width);
        renderLayerInfo.layerProjectionViews[i].subImage.imageRect.extent.height = static_cast<int32_t>(height);
        renderLayerInfo.layerProjectionViews[i].subImage.imageArrayIndex = i; // this eye's layer

        const glm::mat4 proj = utils::computeProjectionMatrix(views[i].fov, pCamera->getNear(), pCamera->getFar());
        worldLockedUniform.view[i] = utils::computeWorldLockViewMatrix(
            views[i].pose,
            pCamera->getPosition(),
            pCamera->getOrientation(),
            mInputHandler_.getHeadPose()
        );
        worldLockedUniform.proj[i] = proj;
        mEyeViewProj_[i] = proj * worldLockedUniform.view[i];
        headLockedUniform.view[i] = utils::computeHeadLockViewMatrix(views[i].pose);
        headLockedUniform.proj[i] = proj;
    }
    if (stereoViews == 1) mEyeViewProj_[1] = mEyeViewProj_[0];
    gfx.mWorldLockedCameraUniform_->setData(&worldLockedUniform, sizeof(worldLockedUniform));
    gfx.mHeadLockedCameraUniform_->setData(&headLockedUniform, sizeof(headLockedUniform));

    // VR: a background color. AR (passthrough blend): black.
    const std::array<float, 4> clearColor =
        (mXRSystem_->m_environmentBlendMode == XR_ENVIRONMENT_BLEND_MODE_OPAQUE)
            ? std::array<float, 4>{0.17f, 0.17f, 0.17f, 1.0f}
            : std::array<float, 4>{0.0f, 0.0f, 0.0f, 1.0f};

    VkImageView colorView = colorSwapchainInfo.imageViews[colorImageIndex];
    gfx.SetRenderAttachments(&colorView, 1, depthSwapchainInfo.imageViews[depthImageIndex],
                             width, height, clearColor, 1.0f);

    clay::GraphicsContextXR::Viewport viewport = {0.0f, 0.0f, (float) width, (float) height, 0.0f, 1.0f};
    clay::GraphicsContextXR::Rect2D scissor = {{(int32_t) 0, (int32_t) 0}, {width, height}};
    gfx.SetViewports(&viewport, 1);
    gfx.SetScissors(&scissor, 1);

    mScenes_.front()->render(cmd);

    // Submit (no CPU wait), then hand the images back: the compositor's reads are ordered
    // after our submission on the same queue.
    gfx.EndRendering();
    XrSwapchainImageReleaseInfo releaseInfo{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
    xrReleaseSwapchainImage(colorSwapchainInfo.swapchain, &releaseInfo);
    xrReleaseSwapchainImage(depthSwapchainInfo.swapchain, &releaseInfo);

    // Fill out the XrCompositionLayerProjection structure for usage with xrEndFrame().
    renderLayerInfo.layerProjectionViews.resize(stereoViews);
    renderLayerInfo.layerProjection.layerFlags =
        XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT |
        XR_COMPOSITION_LAYER_CORRECT_CHROMATIC_ABERRATION_BIT;
    renderLayerInfo.layerProjection.space = mXRSystem_->m_localSpace;
    renderLayerInfo.layerProjection.viewCount = static_cast<uint32_t>(renderLayerInfo.layerProjectionViews.size());
    renderLayerInfo.layerProjection.views = renderLayerInfo.layerProjectionViews.data();

    return true;
}

void AppXR::InitImguiRender() {
    // The GUI pass is recorded into the frame's command buffer (see RenderLayer).
    // Disabled: a token target, only so ImGui (still initialised) has a render pass.
    const uint32_t width = mImguiPanelEnabled_ ? imguiWidth : 16;
    const uint32_t height = mImguiPanelEnabled_ ? imguiHeight : 16;
    mImguiTarget_.create(*mpGraphicsContext_, width, height, {0.0f, 0.0f, 0.0f, 1.0f});
    imguiImageView = mImguiTarget_.getSampledView();
    // ImGui builds its pipeline against this pass.
    mXRSystem_->mpGraphicsContext_->imguiRenderPass = mImguiTarget_.getRenderPass();
}

} // namespace clay

#endif // CLAY_PLATFORM_XR