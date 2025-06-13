#ifdef CLAY_PLATFORM_DESKTOP

// clay
#include "clay/utils/desktop/UtilsDesktop.h"
#include "clay/gui/desktop/ImGuiComponentDesktop.h"
// class
#include "clay/application/desktop/AppDesktop.h"

namespace clay {

AppDesktop::AppDesktop(Window& window) :
    BaseApp(new GraphicsContextDesktop(window)),
    mWindow_(window),
    mGraphicsContextDesktop_((GraphicsContextDesktop&)*mpGraphicsContext_.get()) {
    ImGuiComponentDesktop::initialize(mWindow_, mpGraphicsContext_.get());
}

AppDesktop::~AppDesktop() {
    vkDeviceWaitIdle(mpGraphicsContext_->getDevice()); // wait to allow deleteing current scene
    // delete both scenes
    mSceneBuffer_[0].reset();
    mSceneBuffer_[1].reset();
    ImGuiComponentDesktop::finalize();
    mResources_.releaseAll();
    mGraphicsContextDesktop_.cleanUp();
    // Play "0" audio to clear audio buffer
    mAudioManager_.playSound(0);
}

void AppDesktop::run() {
    while (isRunning()) {
        update();
        render();
    }
    
    vkDeviceWaitIdle(mpGraphicsContext_->getDevice());
}

void AppDesktop::update() {
    std::chrono::duration<float> dt = (std::chrono::steady_clock::now() - mLastTime_);
    mLastTime_ = std::chrono::steady_clock::now();
    mWindow_.update(dt.count());

    // handle window events
    while (auto eventOpt = mWindow_.pollEvent()) {
        const Window::WindowEvent& event = *eventOpt;

        switch (event) {
            case Window::WindowEvent::RESIZED:
                mFramebufferResized_  = true;
                break;
            default:
                break;
        }
    }

    if (mSceneBuffer_[1]) {
        vkDeviceWaitIdle(mpGraphicsContext_->getDevice()); // wait to allow deleteing current scene
        // switch to scene in back buffer
        mSceneBuffer_[0] = std::move(mSceneBuffer_[1]);
    }

    if (mSceneBuffer_[0] != nullptr) {
        mSceneBuffer_[0]->update(dt.count());
    }
}

void AppDesktop::render() {
    vkWaitForFences(mGraphicsContextDesktop_.getDevice(), 1, &mGraphicsContextDesktop_.mInFlightFences_[mGraphicsContextDesktop_.mCurrentFrame_], VK_TRUE, UINT64_MAX);

    uint32_t imageIndex;
    VkResult result = vkAcquireNextImageKHR(
        mpGraphicsContext_->getDevice(), 
        mGraphicsContextDesktop_.mSwapChain_, 
        UINT64_MAX,
        mGraphicsContextDesktop_.mImageAvailableSemaphores_[mGraphicsContextDesktop_.mCurrentFrame_],
        VK_NULL_HANDLE, 
        &imageIndex
    );

    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        mGraphicsContextDesktop_.recreateSwapChain(mWindow_);
        return;
    } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        throw std::runtime_error("failed to acquire swap chain image!");
    }

    vkResetFences(mpGraphicsContext_->getDevice(), 1, &mGraphicsContextDesktop_.mInFlightFences_[mGraphicsContextDesktop_.mCurrentFrame_]);

    vkResetCommandBuffer(mGraphicsContextDesktop_.mCommandBuffers_[mGraphicsContextDesktop_.mCurrentFrame_], /*VkCommandBufferResetFlagBits*/ 0);
    recordCommandBuffer(mGraphicsContextDesktop_.mCommandBuffers_[mGraphicsContextDesktop_.mCurrentFrame_], imageIndex);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    VkSemaphore waitSemaphores[] = {mGraphicsContextDesktop_.mImageAvailableSemaphores_[mGraphicsContextDesktop_.mCurrentFrame_]};
    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;

    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &mGraphicsContextDesktop_.mCommandBuffers_[mGraphicsContextDesktop_.mCurrentFrame_];

    VkSemaphore signalSemaphores[] = {mGraphicsContextDesktop_.mRenderFinishedSemaphores_[mGraphicsContextDesktop_.mCurrentFrame_]};
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    if (vkQueueSubmit(mGraphicsContextDesktop_.mGraphicsQueue_, 1, &submitInfo, mGraphicsContextDesktop_.mInFlightFences_[mGraphicsContextDesktop_.mCurrentFrame_]) != VK_SUCCESS) {
        throw std::runtime_error("failed to submit draw command buffer!");
    }

    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;

    VkSwapchainKHR swapChains[] = {mGraphicsContextDesktop_.mSwapChain_};
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapChains;

    presentInfo.pImageIndices = &imageIndex;

    result = vkQueuePresentKHR(mGraphicsContextDesktop_.mPresentQueue_, &presentInfo);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || mFramebufferResized_) {
        mFramebufferResized_ = false;
        mGraphicsContextDesktop_.recreateSwapChain(mWindow_);
    } else if (result != VK_SUCCESS) {
        throw std::runtime_error("failed to present swap chain image!");
    }

    mGraphicsContextDesktop_.mCurrentFrame_ = (mGraphicsContextDesktop_.mCurrentFrame_ + 1) % GraphicsContextDesktop::MAX_FRAMES_IN_FLIGHT;
}

void AppDesktop::quit() {
    glfwSetWindowShouldClose(mWindow_.getGLFWWindow(), true);
}

void AppDesktop::setScene(BaseScene* pScene) {
    // assign to back buffer
    mSceneBuffer_[1] = std::unique_ptr<BaseScene>(pScene);
}

bool AppDesktop::isRunning() {
    return mWindow_.isRunning();
}

Window& AppDesktop::getWindow() {
    return mWindow_;
}

GraphicsContextDesktop& AppDesktop::getGraphicsContextDesktop() {
    return mGraphicsContextDesktop_;
}

void AppDesktop::recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex) {
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
        throw std::runtime_error("failed to begin recording command buffer!");
    }

    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = mpGraphicsContext_->mRenderPass_;
    renderPassInfo.framebuffer = mGraphicsContextDesktop_.mSwapChainFramebuffers_[imageIndex];
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = mGraphicsContextDesktop_.mSwapChainExtent_;

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
        viewport.width = (float)mGraphicsContextDesktop_.mSwapChainExtent_.width;
        viewport.height = (float)mGraphicsContextDesktop_.mSwapChainExtent_.height;
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

        VkRect2D scissor{};
        scissor.offset = {0, 0};
        scissor.extent = mGraphicsContextDesktop_.mSwapChainExtent_;
        vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

        mSceneBuffer_[0]->render(commandBuffer);
    }
    vkCmdEndRenderPass(commandBuffer);

    if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
        throw std::runtime_error("failed to record command buffer!");
    }
}

} // namespace clay

#endif // CLAY_PLATFORM_DESKTOP