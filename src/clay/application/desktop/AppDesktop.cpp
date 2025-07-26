#ifdef CLAY_PLATFORM_DESKTOP

#include <iostream>
// clay
#include "clay/utils/desktop/UtilsDesktop.h"
#include "clay/gui/desktop/ImGuiComponentDesktop.h"
#include "clay/utils/common/Logger.h"
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
    mpGraphicsContext_->getDevice().waitIdle(); // wait to allow deleteing current scene
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
    
    mpGraphicsContext_->getDevice().waitIdle();
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
        mpGraphicsContext_->getDevice().waitIdle(); // wait to allow deleteing current scene
        // switch to scene in back buffer
        mSceneBuffer_[0] = std::move(mSceneBuffer_[1]);
    }

    if (mSceneBuffer_[0] != nullptr) {
        mSceneBuffer_[0]->update(dt.count());
    }
}

void AppDesktop::render() {
    mGraphicsContextDesktop_.getDevice().waitForFences(
        1, 
        &mGraphicsContextDesktop_.mInFlightFences_[mGraphicsContextDesktop_.mCurrentFrame_], 
        vk::True, 
        UINT64_MAX
    );

    if (tempVSyncFlag) {
        mGraphicsContextDesktop_.setVSync(tempVSyncValue);
        mGraphicsContextDesktop_.recreateSwapChain(mWindow_);
        tempVSyncFlag = false;
    }

    uint32_t imageIndex;

    vk::Result result = mpGraphicsContext_->getDevice().acquireNextImageKHR(
        mGraphicsContextDesktop_.mSwapChain_, 
        UINT64_MAX,
        mGraphicsContextDesktop_.mImageAvailableSemaphores_[mGraphicsContextDesktop_.mCurrentFrame_],
        nullptr, 
        &imageIndex
     );

    if (result == vk::Result::eErrorOutOfDateKHR) {
        mGraphicsContextDesktop_.recreateSwapChain(mWindow_);
        return;
    } else if (result != vk::Result::eSuccess && result != vk::Result::eSuboptimalKHR) {
        throw std::runtime_error("failed to acquire swap chain image!");
    }

    mpGraphicsContext_->getDevice().resetFences(1, &mGraphicsContextDesktop_.mInFlightFences_[mGraphicsContextDesktop_.mCurrentFrame_]);
    mGraphicsContextDesktop_.mCommandBuffers_[mGraphicsContextDesktop_.mCurrentFrame_].reset();

    recordCommandBuffer(mGraphicsContextDesktop_.mCommandBuffers_[mGraphicsContextDesktop_.mCurrentFrame_], imageIndex);

    vk::Semaphore waitSemaphores[] = {mGraphicsContextDesktop_.mImageAvailableSemaphores_[mGraphicsContextDesktop_.mCurrentFrame_]};
    vk::Semaphore signalSemaphores[] = {mGraphicsContextDesktop_.mRenderFinishedSemaphores_[mGraphicsContextDesktop_.mCurrentFrame_]};
    vk::PipelineStageFlags waitStages[] = {vk::PipelineStageFlagBits::eColorAttachmentOutput};
    
    vk::SubmitInfo submitInfo{
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = waitSemaphores,
        .pWaitDstStageMask = waitStages,
        .commandBufferCount = 1,
        .pCommandBuffers = &mGraphicsContextDesktop_.mCommandBuffers_[mGraphicsContextDesktop_.mCurrentFrame_],
        .signalSemaphoreCount = 1,
        .pSignalSemaphores = signalSemaphores,
    };

    mGraphicsContextDesktop_.mGraphicsQueue_.submit(1, &submitInfo, mGraphicsContextDesktop_.mInFlightFences_[mGraphicsContextDesktop_.mCurrentFrame_]);

    vk::SwapchainKHR swapChains[] = {mGraphicsContextDesktop_.mSwapChain_};
    vk::PresentInfoKHR presentInfo{
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = signalSemaphores,
        .swapchainCount = 1,
        .pSwapchains = swapChains,
        .pImageIndices = &imageIndex,
    };

    result = mGraphicsContextDesktop_.mPresentQueue_.presentKHR(presentInfo);
    if (result == vk::Result::eErrorOutOfDateKHR || result == vk::Result::eSuboptimalKHR || mFramebufferResized_) {
        mFramebufferResized_ = false;
        mGraphicsContextDesktop_.recreateSwapChain(mWindow_);
    } else if (result != vk::Result::eSuccess) {
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

void AppDesktop::recordCommandBuffer(vk::CommandBuffer commandBuffer, uint32_t imageIndex) {
    commandBuffer.begin(vk::CommandBufferBeginInfo{});

    vk::RenderPassBeginInfo renderPassInfo{
        .renderPass = mpGraphicsContext_->mRenderPass_,
        .framebuffer = mGraphicsContextDesktop_.mSwapChainFramebuffers_[imageIndex],
        .renderArea = {
            .offset = {0, 0},
            .extent = mGraphicsContextDesktop_.mSwapChainExtent_,
        }
    };

    std::array<vk::ClearValue, 2> clearValues{};
    clearValues[0].color = vk::ClearColorValue(std::array<float, 4>{0.0f, 0.0f, 0.0f, 1.0f});
    clearValues[1].depthStencil = vk::ClearDepthStencilValue(1.0f, 0);

    renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
    renderPassInfo.pClearValues = clearValues.data();

    commandBuffer.beginRenderPass(renderPassInfo, vk::SubpassContents::eInline);
    {
        vk::Viewport viewport{
            viewport.x = 0.0f,
            viewport.y = 0.0f,
            viewport.width = (float)mGraphicsContextDesktop_.mSwapChainExtent_.width,
            viewport.height = (float)mGraphicsContextDesktop_.mSwapChainExtent_.height,
            viewport.minDepth = 0.0f,
            viewport.maxDepth = 1.0f
        };

        commandBuffer.setViewport(0, 1, &viewport);

        vk::Rect2D scissor{
            .offset = {0, 0},
            .extent = mGraphicsContextDesktop_.mSwapChainExtent_
        };

        commandBuffer.setScissor(0, 1, &scissor);

        mSceneBuffer_[0]->render(commandBuffer);
    }
    commandBuffer.endRenderPass();
    commandBuffer.end();
}

} // namespace clay

#endif // CLAY_PLATFORM_DESKTOP