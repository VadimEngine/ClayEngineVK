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

    while (mXRSystem_->m_applicationRunning) {
        mXRSystem_->PollSystemEvents();
        mXRSystem_->PollEvents();
        if (mXRSystem_->m_sessionRunning) {
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

    // TODO use real time
    if (mScenes_.size() > 1) {
        mScenes_.erase(mScenes_.begin(), std::prev(mScenes_.end()));
        mScenes_.front()->initialize();
    }
    mScenes_.front()->update(0);
    // draw imgui onto a different frame buffer

    vkWaitForFences(mXRSystem_->mpGraphicsContext_->getDevice(), 1, &mXRSystem_->mpGraphicsContext_->fence, true, UINT64_MAX);

    if (vkResetFences(mXRSystem_->mpGraphicsContext_->getDevice(), 1, &mXRSystem_->mpGraphicsContext_->fence) != VK_SUCCESS) {
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
    renderPassBegin.renderPass = mXRSystem_->mpGraphicsContext_->imguiRenderPass;
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
    submitInfo.waitSemaphoreCount = mXRSystem_->mpGraphicsContext_->acquireSemaphore ? 1 : 0;
    submitInfo.pWaitSemaphores = mXRSystem_->mpGraphicsContext_->acquireSemaphore ? &mXRSystem_->mpGraphicsContext_->acquireSemaphore : nullptr;
    submitInfo.pWaitDstStageMask = mXRSystem_->mpGraphicsContext_->acquireSemaphore ? &waitDstStageMask : nullptr;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &imguiCommandBuffer;
    submitInfo.signalSemaphoreCount = mXRSystem_->mpGraphicsContext_->submitSemaphore ? 1 : 0;
    submitInfo.pSignalSemaphores = mXRSystem_->mpGraphicsContext_->submitSemaphore ? &mXRSystem_->mpGraphicsContext_->submitSemaphore : nullptr;

    vkQueueSubmit(mXRSystem_->mpGraphicsContext_->mGraphicsQueue_, 1, &submitInfo, mXRSystem_->mpGraphicsContext_->fence);
    vkWaitForFences(mXRSystem_->mpGraphicsContext_->getDevice(), 1, &mXRSystem_->mpGraphicsContext_->fence, true, UINT64_MAX);

    mXRSystem_->mpGraphicsContext_->transitionImageLayout(
        imguiImage,
        VK_FORMAT_R8G8B8A8_UNORM,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        1
    );
    // imgui end

    // Per view in the view configuration:
    for (uint32_t i = 0; i < viewCount; i++) {
        XRSystem::SwapchainInfo &colorSwapchainInfo = mXRSystem_->m_colorSwapchainInfos[i];
        XRSystem::SwapchainInfo &depthSwapchainInfo = mXRSystem_->m_depthSwapchainInfos[i];

        // Acquire and wait for an image from the swapchains.
        // Get the image index of an image in the swapchains.
        // The timeout is infinite.
        uint32_t colorImageIndex = 0;
        uint32_t depthImageIndex = 0;
        XrSwapchainImageAcquireInfo acquireInfo{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
        xrAcquireSwapchainImage(colorSwapchainInfo.swapchain, &acquireInfo, &colorImageIndex);
        xrAcquireSwapchainImage(depthSwapchainInfo.swapchain, &acquireInfo, &depthImageIndex);

        XrSwapchainImageWaitInfo waitInfo = {XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};
        waitInfo.timeout = XR_INFINITE_DURATION;
        xrWaitSwapchainImage(colorSwapchainInfo.swapchain, &waitInfo);
        xrWaitSwapchainImage(depthSwapchainInfo.swapchain, &waitInfo);

        // Get the width and height and construct the viewport and scissors.
        const uint32_t &width = mXRSystem_->m_viewConfigurationViews[i].recommendedImageRectWidth;
        const uint32_t &height = mXRSystem_->m_viewConfigurationViews[i].recommendedImageRectHeight;
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
        mXRSystem_->mpGraphicsContext_->BeginRendering();

        if (mXRSystem_->m_environmentBlendMode == XR_ENVIRONMENT_BLEND_MODE_OPAQUE) {
            // VR mode use a background color.
            mXRSystem_->mpGraphicsContext_->ClearColor(
                colorSwapchainInfo.imageViews[colorImageIndex], 0.17f, 0.17f, 0.17f, 1.00f
            );
        } else {
            // In AR mode make the background color black.
            mXRSystem_->mpGraphicsContext_->ClearColor(
                colorSwapchainInfo.imageViews[colorImageIndex], 0.00f, 0.00f, 0.00f, 1.00f
            );
        }
        mXRSystem_->mpGraphicsContext_->ClearDepth(depthSwapchainInfo.imageViews[depthImageIndex], 1.0f);

        mXRSystem_->mpGraphicsContext_->SetRenderAttachments(
            &colorSwapchainInfo.imageViews[colorImageIndex], 1,
            depthSwapchainInfo.imageViews[depthImageIndex], width,
            height
        );
        mXRSystem_->mpGraphicsContext_->SetViewports(&viewport, 1);
        mXRSystem_->mpGraphicsContext_->SetScissors(&scissor, 1);

        // Compute the view-projection transform.
        const Camera* pCamera = mScenes_.front()->getFocusCamera();
        const glm::mat4 glmProj = utils::computeProjectionMatrix(views[i].fov, pCamera->getNear(), pCamera->getFar());

        const glm::mat4 glmViewWorldLocked = utils::computeWorldLockViewMatrix(
            views[i].pose,
            pCamera->getPosition(),
            pCamera->getOrientation(),
            mInputHandler_.getHeadPose()
        );
        const glm::mat4 glmViewHeadLocked = utils::computeHeadLockViewMatrix(views[i].pose);

        struct {
            glm::mat4 view;
            glm::mat4 proj;
        } worldLockedUniform { glmViewWorldLocked, glmProj };

        struct {
            glm::mat4 view;
            glm::mat4 proj;
        } headLockedUniform { glmViewHeadLocked, glmProj };

        mXRSystem_->mpGraphicsContext_->mWorldLockedCameraUniform_->setData(&worldLockedUniform, sizeof(worldLockedUniform));
        mXRSystem_->mpGraphicsContext_->mHeadLockedCameraUniform_->setData(&headLockedUniform, sizeof(headLockedUniform));

        mScenes_.front()->render(mXRSystem_->mpGraphicsContext_->cmdBuffer);

        mXRSystem_->mpGraphicsContext_->EndRendering();
        // Give the swapchain image back to OpenXR, allowing the compositor to use the image.
        XrSwapchainImageReleaseInfo releaseInfo{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
        xrReleaseSwapchainImage(colorSwapchainInfo.swapchain, &releaseInfo);
        xrReleaseSwapchainImage(depthSwapchainInfo.swapchain, &releaseInfo);
    }

    // Fill out the XrCompositionLayerProjection structure for usage with xrEndFrame().
    renderLayerInfo.layerProjection.layerFlags =
        XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT |
        XR_COMPOSITION_LAYER_CORRECT_CHROMATIC_ABERRATION_BIT;
    renderLayerInfo.layerProjection.space = mXRSystem_->m_localSpace;
    renderLayerInfo.layerProjection.viewCount = static_cast<uint32_t>(renderLayerInfo.layerProjectionViews.size());
    renderLayerInfo.layerProjection.views = renderLayerInfo.layerProjectionViews.data();

    // transition imgui back
    mXRSystem_->mpGraphicsContext_->transitionImageLayout(
        imguiImage,
        VK_FORMAT_R8G8B8A8_UNORM,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        1
    );

    return true;
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
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

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

    if (vkCreateRenderPass(mpGraphicsContext_->getDevice(), &renderPassInfo, nullptr, &mXRSystem_->mpGraphicsContext_->imguiRenderPass) != VK_SUCCESS) {
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
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        imguiImage,
        imguiImageMemory
    );

    mXRSystem_->mpGraphicsContext_->transitionImageLayout(
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
    framebufferInfo.renderPass = mXRSystem_->mpGraphicsContext_->imguiRenderPass;
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