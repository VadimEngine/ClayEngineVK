#ifdef CLAY_PLATFORM_ANDROID
// third party
#include <android/asset_manager.h>
#include <android/bitmap.h>
#include <android/imagedecoder.h>
// clay
#include "clay/application/android/AppAndroid.h"
#include "clay/gui/android/ImGuiComponentAndroid.h"
#include "clay/utils/common/Utils.h"

namespace clay {

AppAndroid::AppAndroid(android_app* app)
    : BaseApp(new GraphicsContextAndroid(app)),
      mAndroidNativeApp_(app) {
    ImGuiComponentAndroid::initialize(app, mpGraphicsContext_.get());
}

AppAndroid::~AppAndroid() {
    // delete both scenes before freeing other resources
    mSceneBuffer_[0].reset();
    mSceneBuffer_[1].reset();
}

void AppAndroid::loadResources() {}

void AppAndroid::update() {
    std::chrono::duration<float> dt = (std::chrono::steady_clock::now() - mLastTime_);
    mLastTime_ = std::chrono::steady_clock::now();

    if (mSceneBuffer_[1]) {
        vkDeviceWaitIdle(mpGraphicsContext_->getDevice()); // wait to allow deleteing current scene
        // switch to scene in back buffer
        mSceneBuffer_[0] = std::move(mSceneBuffer_[1]);
    }

    if (mSceneBuffer_[0] != nullptr) {
        mSceneBuffer_[0]->update(dt.count());
    }
}

void AppAndroid::render() {
    vkWaitForFences(mpGraphicsContext_->getDevice(), 1, &((GraphicsContextAndroid*)mpGraphicsContext_.get())->mInFlightFences_[((GraphicsContextAndroid*)mpGraphicsContext_.get())->mCurrentFrame_], VK_TRUE, UINT64_MAX);

    uint32_t imageIndex;
    VkResult result = vkAcquireNextImageKHR(
        mpGraphicsContext_->getDevice(),
        ((GraphicsContextAndroid*)mpGraphicsContext_.get())->mSwapChain_,
        UINT64_MAX,
        ((GraphicsContextAndroid*)mpGraphicsContext_.get())->mImageAvailableSemaphores_[((GraphicsContextAndroid*)mpGraphicsContext_.get())->mCurrentFrame_],
        VK_NULL_HANDLE,
        &imageIndex
    );

//    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
//        mpGraphicsContext_->recreateSwapChain(mWindow_);
//        return;
//    } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
//        throw std::runtime_error("failed to acquire swap chain image!");
//    }

    vkResetFences(mpGraphicsContext_->getDevice(), 1, &((GraphicsContextAndroid*)mpGraphicsContext_.get())->mInFlightFences_[((GraphicsContextAndroid*)mpGraphicsContext_.get())->mCurrentFrame_]);

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
    submitInfo.pCommandBuffers = &((GraphicsContextAndroid*)mpGraphicsContext_.get())->mCommandBuffers_[((GraphicsContextAndroid*)mpGraphicsContext_.get())->mCurrentFrame_];

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

//    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || mFramebufferResized_) {
//        mFramebufferResized_ = false;
//        mGraphicsContextDesktop_.recreateSwapChain(mWindow_);
//    } else if (result != VK_SUCCESS) {
//        throw std::runtime_error("failed to present swap chain image!");
//    }

    ((GraphicsContextAndroid*)mpGraphicsContext_.get())->mCurrentFrame_ = (((GraphicsContextAndroid*)mpGraphicsContext_.get())->mCurrentFrame_ + 1) % GraphicsContextAndroid::MAX_FRAMES_IN_FLIGHT;
}

void AppAndroid::recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex) {
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
        throw std::runtime_error("failed to begin recording command buffer!");
    }

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
    size_t fileSize = AAsset_getLength(asset);

    auto buffer = std::make_unique<unsigned char[]>(fileSize);
    AAsset_read(asset, buffer.get(), fileSize);
    AAsset_close(asset);

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

} // namespace clay
#endif // CLAY_PLATFORM_ANDROID