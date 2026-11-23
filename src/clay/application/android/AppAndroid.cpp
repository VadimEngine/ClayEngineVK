#ifdef CLAY_PLATFORM_ANDROID
// third party
#include <android/asset_manager.h>
#include <android/bitmap.h>
#include <android/imagedecoder.h>
// clay
#include "clay/application/android/AppAndroid.h"
#include "clay/gui/android/ImGuiComponentAndroid.h"
#include "clay/gui/android/UiComponentAndroid.h"
#include "clay/application/common/Resources.h"
#include "clay/utils/common/Utils.h"

namespace clay {

AppAndroid::AppAndroid(android_app* app)
    : BaseApp(new GraphicsContextAndroid(app)),
      mAndroidNativeApp_(app) {
    // Files come from the APK's assets unless the app sets another loader.
    Resources::setFileLoader([this](const std::string& path) { return loadFileToMemory(path); });
    mLastTime_ = std::chrono::steady_clock::now();
    mStartTime_ = mLastTime_;
    ImGuiComponentAndroid::initialize(app, mpGraphicsContext_.get());
    // The game UI on RmlUi (no-op unless built with CLAY_ENABLE_RMLUI): views and fonts from
    // the assets' ui folder.
    UiComponentAndroid::initialize(app, mpGraphicsContext_.get(), "ui");
}

AppAndroid::~AppAndroid() {
    // delete both scenes before freeing other resources
    mSceneBuffer_[0].reset();
    mSceneBuffer_[1].reset();

    UiComponentAndroid::finalize();
    // Shutdown ImGui
    ImGuiComponentAndroid::finalize();
}

void AppAndroid::loadResources() {}

void AppAndroid::update() {
    const auto now = std::chrono::steady_clock::now();
    // Clamped like the desktop app: a long stall (the app in the background, a slow first
    // frame) must not throw the simulation forward.
    const float dtSeconds = std::min(std::chrono::duration<float>(now - mLastTime_).count(), 0.1f);
    mLastTime_ = now;

    if (mSceneBuffer_[1]) {
        vkDeviceWaitIdle(mpGraphicsContext_->getDevice()); // wait to allow deleting current scene
        // switch to scene in back buffer
        mSceneBuffer_[0] = std::move(mSceneBuffer_[1]);
        // Initialize the new scene (as AppDesktop does).
        mSceneBuffer_[0]->initialize();
    }

    if (mSceneBuffer_[0] != nullptr) {
        mSceneBuffer_[0]->update(dtSeconds);
    }
    UiComponentAndroid::update(std::chrono::duration<double>(now - mStartTime_).count());
}

void AppAndroid::render() {
    mpGraphicsContext_->getDevice().waitForFences(
        1,
        &((GraphicsContextAndroid*)mpGraphicsContext_.get())->mInFlightFences_[((GraphicsContextAndroid*)mpGraphicsContext_.get())->mCurrentFrame_],
        VK_TRUE,
        UINT64_MAX
    );

    uint32_t imageIndex;
    VkResult result = vkAcquireNextImageKHR(
        mpGraphicsContext_->getDevice(),
        ((GraphicsContextAndroid*)mpGraphicsContext_.get())->mSwapChain_,
        UINT64_MAX,
        ((GraphicsContextAndroid*)mpGraphicsContext_.get())->mImageAvailableSemaphores_[((GraphicsContextAndroid*)mpGraphicsContext_.get())->mCurrentFrame_],
        VK_NULL_HANDLE,
        &imageIndex
    );

    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        if (mAndroidNativeApp_->window != nullptr) {
            ((GraphicsContextAndroid*)mpGraphicsContext_.get())->recreateSwapChain(mAndroidNativeApp_);
        }
        return;
    } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        throw std::runtime_error("failed to acquire swap chain image!");
    }

    mpGraphicsContext_->getDevice().resetFences(1, &((GraphicsContextAndroid*)mpGraphicsContext_.get())->mInFlightFences_[((GraphicsContextAndroid*)mpGraphicsContext_.get())->mCurrentFrame_]);
    mpGraphicsContext_->beginFrame(((GraphicsContextAndroid*)mpGraphicsContext_.get())->mCurrentFrame_);

    vkResetCommandBuffer(((GraphicsContextAndroid*)mpGraphicsContext_.get())->mCommandBuffers_[((GraphicsContextAndroid*)mpGraphicsContext_.get())->mCurrentFrame_], /*VkCommandBufferResetFlagBits*/ 0);
    recordCommandBuffer(((GraphicsContextAndroid*)mpGraphicsContext_.get())->mCommandBuffers_[((GraphicsContextAndroid*)mpGraphicsContext_.get())->mCurrentFrame_], imageIndex);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    VkSemaphore waitSemaphores[] = {((GraphicsContextAndroid*)mpGraphicsContext_.get())->mImageAvailableSemaphores_[((GraphicsContextAndroid*)mpGraphicsContext_.get())->mCurrentFrame_]};
    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;

    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers =
        reinterpret_cast<const VkCommandBuffer*>(
            &((GraphicsContextAndroid*)mpGraphicsContext_.get())->mCommandBuffers_[
                ((GraphicsContextAndroid*)mpGraphicsContext_.get())->mCurrentFrame_
            ]
        );

    VkSemaphore signalSemaphores[] = {((GraphicsContextAndroid*)mpGraphicsContext_.get())->mRenderFinishedSemaphores_[((GraphicsContextAndroid*)mpGraphicsContext_.get())->mCurrentFrame_]};
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    if (vkQueueSubmit(mpGraphicsContext_->mGraphicsQueue_, 1, &submitInfo, ((GraphicsContextAndroid*)mpGraphicsContext_.get())->mInFlightFences_[((GraphicsContextAndroid*)mpGraphicsContext_.get())->mCurrentFrame_]) != VK_SUCCESS) {
        throw std::runtime_error("failed to submit draw command buffer!");
    }

    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;

    VkSwapchainKHR swapChains[] = {((GraphicsContextAndroid*)mpGraphicsContext_.get())->mSwapChain_};
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapChains;

    presentInfo.pImageIndices = &imageIndex;

    result = vkQueuePresentKHR(((GraphicsContextAndroid*)mpGraphicsContext_.get())->mPresentQueue_, &presentInfo);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        if (mAndroidNativeApp_->window != nullptr) {
            ((GraphicsContextAndroid*)mpGraphicsContext_.get())->recreateSwapChain(mAndroidNativeApp_);
        }
    } else if (result != VK_SUCCESS) {
        throw std::runtime_error("failed to present swap chain image!");
    }

    ((GraphicsContextAndroid*)mpGraphicsContext_.get())->mCurrentFrame_ = (((GraphicsContextAndroid*)mpGraphicsContext_.get())->mCurrentFrame_ + 1) % GraphicsContextAndroid::MAX_FRAMES_IN_FLIGHT;
}

void AppAndroid::recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex) {
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
        throw std::runtime_error("failed to begin recording command buffer!");
    }

    // The UI's offscreen work (app-filled images) goes before the main pass.
    UiComponentAndroid::renderOffscreen(commandBuffer);

    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = mpGraphicsContext_->mRenderPass_;
    renderPassInfo.framebuffer = ((GraphicsContextAndroid*)mpGraphicsContext_.get())->mSwapChainFramebuffers_[imageIndex];
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = ((GraphicsContextAndroid*)mpGraphicsContext_.get())->mSwapChainExtent_;

    std::array<VkClearValue, 2> clearValues{};
    clearValues[0].color = {{0.0f, 0.0f, 0.0f, 1.0f}};
    clearValues[1].depthStencil = {1.0f, 0};

    renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
    renderPassInfo.pClearValues = clearValues.data();

    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
    {
        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = (float)((GraphicsContextAndroid*)mpGraphicsContext_.get())->mSwapChainExtent_.width;
        viewport.height = (float)((GraphicsContextAndroid*)mpGraphicsContext_.get())->mSwapChainExtent_.height;
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

        VkRect2D scissor{};
        scissor.offset = {0, 0};
        scissor.extent = ((GraphicsContextAndroid*)mpGraphicsContext_.get())->mSwapChainExtent_;
        vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

        if (mSceneBuffer_[0] != nullptr) {
            mSceneBuffer_[0]->render(commandBuffer);
        }
        // The game UI over the scene.
        UiComponentAndroid::render(commandBuffer);
    }
    vkCmdEndRenderPass(commandBuffer);

    if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
        throw std::runtime_error("failed to record command buffer!");
    }
}

void AppAndroid::setScene(BaseScene* pScene) {
    // assign to back buffer
    mSceneBuffer_[1] = std::unique_ptr<BaseScene>(pScene);
}

utils::FileData AppAndroid::loadFileToMemory(const std::filesystem::path& filePath) {
    auto *assetManager = mAndroidNativeApp_->activity->assetManager;
    AAsset *asset = AAssetManager_open(assetManager, filePath.c_str(), AASSET_MODE_BUFFER);
    if (!asset) {
        throw std::runtime_error("Failed to open asset: " + filePath.string());
    }
    const size_t fileSize = static_cast<size_t>(AAsset_getLength(asset));

    // AAsset_read may return less than asked (large compressed assets come in pieces), so
    // read until the whole file is in; a short read used to leave garbage in the buffer.
    auto buffer = std::make_unique<unsigned char[]>(fileSize);
    size_t got = 0;
    while (got < fileSize) {
        const int n = AAsset_read(asset, buffer.get() + got, fileSize - got);
        if (n <= 0) break;
        got += static_cast<size_t>(n);
    }
    AAsset_close(asset);
    if (got != fileSize) {
        throw std::runtime_error("Short read of asset: " + filePath.string());
    }

    return {std::move(buffer), static_cast<std::size_t>(fileSize)};
}

utils::ImageData AppAndroid::loadImageFileToMemory(const std::filesystem::path& filePath) {
    auto *assetManager = mAndroidNativeApp_->activity->assetManager;
    AAsset *asset = AAssetManager_open(assetManager, filePath.c_str(), AASSET_MODE_STREAMING);

    if (!asset) {
        throw std::runtime_error("Failed to open asset file: " + filePath.string());
    }

    AImageDecoder *decoder = nullptr;
    int result = AImageDecoder_createFromAAsset(asset, &decoder);
    if (result != ANDROID_IMAGE_DECODER_SUCCESS) {
        AAsset_close(asset);
        throw std::runtime_error("Failed to create image decoder for file: " + filePath.string());
    }

    utils::ImageData imageData{};

    const AImageDecoderHeaderInfo *info = AImageDecoder_getHeaderInfo(decoder);
    imageData.width = AImageDecoderHeaderInfo_getWidth(info);
    imageData.height = AImageDecoderHeaderInfo_getHeight(info);

    if ((AndroidBitmapFormat) AImageDecoderHeaderInfo_getAndroidBitmapFormat(info) ==
        AndroidBitmapFormat::ANDROID_BITMAP_FORMAT_RGBA_8888) {
        imageData.channels = 4;
    } else {
        throw std::runtime_error("Unsupported Image format: " + filePath.string());
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

bool AppAndroid::handleInputEvent(AInputEvent* event) {
    // Process with input handler
    return mInputHandler_.processInputEvent(event);
}

InputHandlerAndroid& AppAndroid::getInputHandler() {
    return mInputHandler_;
}

void AppAndroid::onWindowRecreated() {
    // A new native window needs a new surface, not just a new swapchain.
    ((GraphicsContextAndroid*)mpGraphicsContext_.get())->recreateSurface(mAndroidNativeApp_);
}

} // namespace clay
#endif // CLAY_PLATFORM_ANDROID