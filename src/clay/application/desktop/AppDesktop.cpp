#ifdef CLAY_PLATFORM_DESKTOP

#include <iostream>
#include <algorithm>
#include <cmath>
#include <string>
#include <cstdio>
#include <cstdlib>
// third party
#include <imgui.h>
// clay
#include "clay/utils/desktop/UtilsDesktop.h"
#include "clay/gui/desktop/ImGuiComponentDesktop.h"
#include "clay/gui/desktop/UiComponentDesktop.h"
#include "clay/application/common/Resources.h"
#include "clay/utils/common/Logger.h"
#include "clay/utils/common/Profiler.h"
// class
#include "clay/application/desktop/AppDesktop.h"

namespace clay {

AppDesktop::AppDesktop(Window& window) :
    BaseApp(new GraphicsContextDesktop(window)),
    mWindow_(window),
    mGraphicsContextDesktop_((GraphicsContextDesktop&)*mpGraphicsContext_.get()) {
    ImGuiComponentDesktop::initialize(mWindow_, mpGraphicsContext_.get());
    // The game UI on RmlUi (no-op unless built with CLAY_ENABLE_RMLUI). Views/fonts load from
    // the app's res/ui folder.
    UiComponentDesktop::initialize(mWindow_, mpGraphicsContext_.get(),
        (Resources::getResourcePath() / "ui").string().c_str());
}

AppDesktop::~AppDesktop() {
    mpGraphicsContext_->getDevice().waitIdle(); // wait to allow deleteing current scene
    // delete both scenes
    mSceneBuffer_[0].reset();
    mSceneBuffer_[1].reset();
    UiComponentDesktop::finalize();
    ImGuiComponentDesktop::finalize();
    // Clear audio source before deleting audio resources
    mAudioManager_.clearSource();
    mResources_.releaseAll();
    mGraphicsContextDesktop_.cleanUp();
}

void AppDesktop::run() {
    // Loading phase: let the app load heavy resources while we draw a progress bar.
    // Both hooks default to no-ops, so apps that load in their constructor skip this.
    loadResourcesWithProgress(
        [this](float progress, const std::string& label) { renderLoadingProgress(progress, label); }
    );
    onResourcesLoaded();

    // Reset the frame clock so the first real frame doesn't see a multi-second dt
    // from the time spent loading.
    mLastTime_ = std::chrono::steady_clock::now();

    while (isRunning()) {
        update();
        render();
        Profiler::get().endFrame();
    }

    mpGraphicsContext_->getDevice().waitIdle();
}

void AppDesktop::update() {
    std::chrono::duration<float> frameDur = (std::chrono::steady_clock::now() - mLastTime_);
    mLastTime_ = std::chrono::steady_clock::now();

    // Frame-pacing for smooth motion under vsync. CPU-measured dt drifts against
    // the vblank-locked present cadence (worsened by frames-in-flight letting the
    // CPU run ahead), giving alternating long/short frames = visible judder.
    // Fix in two steps: (1) snap the raw dt to the nearest common refresh period
    // so every on-cadence frame integrates with an IDENTICAL step; (2) average a
    // few frames to absorb the occasional off-cadence spike. The running average
    // still equals true frame time, so motion speed is unchanged.
    float rawDt = frameDur.count();
    if (rawDt < 0.0f) rawDt = 0.0f;
    if (rawDt > 0.1f) rawDt = 0.1f;

    // Snap to the nearest vsync cadence (144/120/100/75/60/50/30 Hz) when close.
    static const float kSnaps[] = {
        1.0f / 144.0f, 1.0f / 120.0f, 1.0f / 100.0f, 1.0f / 75.0f,
        1.0f / 60.0f,  1.0f / 50.0f,  1.0f / 30.0f
    };
    for (float s : kSnaps) {
        if (std::fabs(rawDt - s) < 0.0012f) { rawDt = s; break; }
    }

    mDtHistory_[mDtHistoryIndex_] = rawDt;
    mDtHistoryIndex_ = (mDtHistoryIndex_ + 1) % kDtSmoothCount_;
    if (mDtHistoryCount_ < kDtSmoothCount_) ++mDtHistoryCount_;
    float dtSum = 0.0f;
    for (int i = 0; i < mDtHistoryCount_; ++i) dtSum += mDtHistory_[i];
    float dt = dtSum / static_cast<float>(mDtHistoryCount_);

    // Re-snap the averaged value so a steady cadence yields a perfectly constant dt.
    for (float s : kSnaps) {
        if (std::fabs(dt - s) < 0.0012f) { dt = s; break; }
    }

    mWindow_.update(dt);

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
        // Initialize the new scene
        mSceneBuffer_[0]->initialize();
    }

    if (mSceneBuffer_[0] != nullptr) {
        CLAY_PROFILE_SCOPE("Scene update");
        mSceneBuffer_[0]->update(dt);
    }

    // Advance the UI clock (absolute, monotonically increasing seconds).
    static double uiTime = 0.0;
    uiTime += dt;
    {
        CLAY_PROFILE_SCOPE("UI update");
        UiComponentDesktop::update(uiTime);
    }
}

void AppDesktop::render() {
    presentFrame([this](vk::CommandBuffer cmd, uint32_t imageIndex) {
        recordCommandBuffer(cmd, imageIndex);
    });
}

void AppDesktop::presentFrame(const std::function<void(vk::CommandBuffer, uint32_t)>& record) {
    {
        CLAY_PROFILE_SCOPE("GPU wait (fence)");
        mGraphicsContextDesktop_.getDevice().waitForFences(
            1,
            &mGraphicsContextDesktop_.mInFlightFences_[mGraphicsContextDesktop_.mCurrentFrame_],
            vk::True,
            UINT64_MAX
        );
    }

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
    // This slot's previous frame has finished on the GPU: its transient data is reusable.
    mGraphicsContextDesktop_.beginFrame(static_cast<uint32_t>(mGraphicsContextDesktop_.mCurrentFrame_));
    mGraphicsContextDesktop_.mCommandBuffers_[mGraphicsContextDesktop_.mCurrentFrame_].reset();

    {
        CLAY_PROFILE_SCOPE("Record commands");
        record(mGraphicsContextDesktop_.mCommandBuffers_[mGraphicsContextDesktop_.mCurrentFrame_], imageIndex);
    }
    CLAY_PROFILE_SCOPE("Submit + present");

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

    // Dev: CLAY_CAPTURE=<file.png> saves the frame shown CLAY_CAPTURE_AFTER seconds
    // (default 5) after start-up and then quits. CLAY_CAPTURE_COUNT=<n> takes n frames
    // CLAY_CAPTURE_EVERY seconds apart (default 1) as file_0.png, file_1.png, ... to
    // look at motion. Apps can also call requestCapture() themselves.
    static const char* envCapture = std::getenv("CLAY_CAPTURE");
    static int envCaptureTaken = 0;
    static bool envCaptureDone = false;
    if (envCapture && !envCaptureDone && !mGraphicsContextDesktop_.capturePending()) {
        static const double start = glfwGetTime();
        static const char* after = std::getenv("CLAY_CAPTURE_AFTER");
        static const char* every = std::getenv("CLAY_CAPTURE_EVERY");
        static const char* count = std::getenv("CLAY_CAPTURE_COUNT");
        static const double delay = after ? std::atof(after) : 5.0;
        static const double step = every ? std::atof(every) : 1.0;
        static const int total = count ? std::max(1, std::atoi(count)) : 1;
        if (glfwGetTime() - start >= delay + step * envCaptureTaken) {
            std::string path = envCapture;
            if (total > 1) {
                const size_t dot = path.find_last_of('.');
                path.insert(dot == std::string::npos ? path.size() : dot, "_" + std::to_string(envCaptureTaken));
            }
            mGraphicsContextDesktop_.requestCapture(path);
            if (++envCaptureTaken >= total) {
                envCaptureDone = true;
                quit();
            }
        }
    }
    if (mGraphicsContextDesktop_.capturePending()) {
        // The frame must be finished on the GPU before it is read back.
        mGraphicsContextDesktop_.getDevice().waitForFences(
            1, &mGraphicsContextDesktop_.mInFlightFences_[mGraphicsContextDesktop_.mCurrentFrame_], vk::True, UINT64_MAX);
        const std::string path = mGraphicsContextDesktop_.mCapturePath_;
        const bool ok = mGraphicsContextDesktop_.captureSwapchainImage(imageIndex);
        LOG_I("[capture] %s %s", path.c_str(), ok ? "saved" : "FAILED");
        std::fflush(stdout);
    }

    vk::SwapchainKHR swapChains[] = {mGraphicsContextDesktop_.mSwapChain_};
    vk::PresentInfoKHR presentInfo{
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = signalSemaphores,
        .swapchainCount = 1,
        .pSwapchains = swapChains,
        .pImageIndices = &imageIndex,
    };

    // vulkan-hpp throws OutOfDateKHRError (rather than returning the result code)
    // when the swapchain is invalidated — e.g. on minimize/resize — so catch it
    // and fall through to recreate the swapchain instead of crashing.
    try {
        result = mGraphicsContextDesktop_.mPresentQueue_.presentKHR(presentInfo);
    } catch (const vk::OutOfDateKHRError&) {
        result = vk::Result::eErrorOutOfDateKHR;
    }
    if (result == vk::Result::eErrorOutOfDateKHR || result == vk::Result::eSuboptimalKHR || mFramebufferResized_) {
        mFramebufferResized_ = false;
        mGraphicsContextDesktop_.recreateSwapChain(mWindow_);
    } else if (result != vk::Result::eSuccess) {
        throw std::runtime_error("failed to present swap chain image!");
    }

    mGraphicsContextDesktop_.mCurrentFrame_ = (mGraphicsContextDesktop_.mCurrentFrame_ + 1) % GraphicsContextDesktop::MAX_FRAMES_IN_FLIGHT;
}

void AppDesktop::renderLoadingProgress(float progress, const std::string& label) {
    // Pump OS/window events so the window keeps responding (and tracks resizes)
    // between the heavy loading steps that call this.
    mWindow_.update(0.0f);
    while (auto eventOpt = mWindow_.pollEvent()) {
        if (*eventOpt == Window::WindowEvent::RESIZED) mFramebufferResized_ = true;
    }

    // Advance the UI clock so a loading-screen view (if the app loaded one)
    // lays out/animates. Harmless when no view is loaded.
    static double uiLoadTime = 0.0;
    uiLoadTime += 1.0 / 60.0;
    UiComponentDesktop::update(uiLoadTime);

    presentFrame([this, progress, &label](vk::CommandBuffer cmd, uint32_t imageIndex) {
        recordLoadingCommandBuffer(cmd, imageIndex, progress, label);
    });
}

void AppDesktop::recordLoadingCommandBuffer(vk::CommandBuffer commandBuffer, uint32_t imageIndex,
                                            float progress, const std::string& label) {
    commandBuffer.begin(vk::CommandBufferBeginInfo{});

    // UI pre-pass (uploads of app-filled images) MUST run outside the main pass.
    // No-op unless the app loaded a loading-screen view.
    UiComponentDesktop::renderOffscreen(commandBuffer);

    vk::RenderPassBeginInfo renderPassInfo{
        .renderPass = mpGraphicsContext_->mRenderPass_,
        .framebuffer = mGraphicsContextDesktop_.mSwapChainFramebuffers_[imageIndex],
        .renderArea = {
            .offset = {0, 0},
            .extent = mGraphicsContextDesktop_.mSwapChainExtent_,
        }
    };

    std::array<vk::ClearValue, 2> clearValues{};
    clearValues[0].color = vk::ClearColorValue(std::array<float, 4>{0.06f, 0.07f, 0.09f, 1.0f}); // dark
    clearValues[1].depthStencil = vk::ClearDepthStencilValue(1.0f, 0);
    renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
    renderPassInfo.pClearValues = clearValues.data();

    commandBuffer.beginRenderPass(renderPassInfo, vk::SubpassContents::eInline);
    {
        vk::Viewport viewport{
            .x = 0.0f,
            .y = 0.0f,
            .width = (float)mGraphicsContextDesktop_.mSwapChainExtent_.width,
            .height = (float)mGraphicsContextDesktop_.mSwapChainExtent_.height,
            .minDepth = 0.0f,
            .maxDepth = 1.0f
        };
        commandBuffer.setViewport(0, 1, &viewport);

        vk::Rect2D scissor{
            .offset = {0, 0},
            .extent = mGraphicsContextDesktop_.mSwapChainExtent_
        };
        commandBuffer.setScissor(0, 1, &scissor);

        // The UI (the loading view, if the app loaded one).
        UiComponentDesktop::render(commandBuffer);

        ImGuiComponentDesktop::beginRender();
        renderLoadingScreen(progress, label);
        ImGuiComponentDesktop::endRender(commandBuffer);
    }
    commandBuffer.endRenderPass();
    commandBuffer.end();
}

void AppDesktop::renderLoadingScreen(float progress, const std::string& label) {
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->GetCenter(), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(440.0f, 0.0f), ImGuiCond_Always);

    const ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings;

    if (ImGui::Begin("##loading", nullptr, flags)) {
        // Centered title.
        const float titleW = ImGui::CalcTextSize(mLoadingTitle_.c_str()).x;
        ImGui::SetCursorPosX((ImGui::GetWindowSize().x - titleW) * 0.5f);
        ImGui::TextUnformatted(mLoadingTitle_.c_str());

        ImGui::Dummy(ImVec2(0.0f, 10.0f));
        ImGui::ProgressBar(progress, ImVec2(-1.0f, 0.0f));
        ImGui::Dummy(ImVec2(0.0f, 4.0f));
        if (!label.empty()) ImGui::TextDisabled("%s", label.c_str());
    }
    ImGui::End();
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

    // Clear the shadow map every frame, then let the scene render its casters into
    // it. A scene with no shadow pass thus samples a fully-lit (cleared) map rather
    // than stale depth from a previous scene.
    mGraphicsContextDesktop_.beginShadowRenderPass(commandBuffer);
    mGraphicsContextDesktop_.endShadowRenderPass(commandBuffer);
    {
        CLAY_PROFILE_SCOPE("  Shadow pass");
        mSceneBuffer_[0]->renderShadowPass(commandBuffer);
    }

    // UI pre-pass (uploads of app-filled images) MUST run outside the main pass.
    {
        CLAY_PROFILE_SCOPE("  UI pre-pass");
        UiComponentDesktop::renderOffscreen(commandBuffer);
    }

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

        {
            CLAY_PROFILE_SCOPE("  Scene render");
            mSceneBuffer_[0]->render(commandBuffer);
        }

        // Draw the UI into the swapchain pass, after the scene.
        CLAY_PROFILE_SCOPE("  UI render");
        UiComponentDesktop::render(commandBuffer);
    }
    commandBuffer.endRenderPass();
    commandBuffer.end();
}

} // namespace clay

#endif // CLAY_PLATFORM_DESKTOP