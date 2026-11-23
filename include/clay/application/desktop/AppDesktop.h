#pragma once

#ifdef CLAY_PLATFORM_DESKTOP
// standard lib
#include <stdexcept>
#include <cstdlib>
#include <array>
#include <chrono>
#include <functional>
#include <string>
// clay
#include "clay/gui/desktop/Window.h"
#include "clay/application/common/BaseScene.h"
#include "clay/graphics/desktop/GraphicsContextDesktop.h"
#include "clay/application/common/BaseApp.h"

namespace clay {

class AppDesktop : public clay::BaseApp {
public:
    AppDesktop(Window& window);

    virtual ~AppDesktop();

    void run();

    void update();

    void render();

    void quit();

    void setScene(BaseScene* pScene);

    bool isRunning();

    Window& getWindow();

    GraphicsContextDesktop& getGraphicsContextDesktop();

    // Title shown on the default loading screen (e.g. the app/game name).
    void setLoadingTitle(const std::string& title) { mLoadingTitle_ = title; }

    bool tempVSyncFlag = false;
    bool tempVSyncValue = false;


protected:
    void recordCommandBuffer(vk::CommandBuffer commandBuffer, uint32_t imageIndex);

    // Acquire/submit/present scaffolding shared by render() and the loading screen.
    // `record` fills the command buffer (begin render pass, draw, end) for the frame.
    void presentFrame(const std::function<void(vk::CommandBuffer, uint32_t)>& record);

    // Draw + present one loading-screen frame (used as the progress callback while
    // loadResourcesWithProgress runs). Pumps window events so the window stays live.
    void renderLoadingProgress(float progress, const std::string& label);
    void recordLoadingCommandBuffer(vk::CommandBuffer commandBuffer, uint32_t imageIndex,
                                    float progress, const std::string& label);

    // The loading screen UI itself (ImGui, drawn inside the render pass). Override to
    // customize (e.g. draw a logo loaded before loadResourcesWithProgress). Default:
    // a centered title + progress bar + current-step label.
    virtual void renderLoadingScreen(float progress, const std::string& label);

    std::string mLoadingTitle_ = "Loading";

    // Delta-time smoothing: averaging the last few frames' dt removes the
    // alternating-long/short-frame judder that CPU-measured dt shows under
    // strict (FIFO) vsync, where CPU frame starts drift against vblank-locked
    // presents. Over time the average equals the true frame time, so motion
    // speed is unchanged - only the per-frame jitter is damped.
    static constexpr int kDtSmoothCount_ = 4;
    float mDtHistory_[kDtSmoothCount_] = {0.0f, 0.0f, 0.0f, 0.0f};
    int mDtHistoryCount_ = 0;
    int mDtHistoryIndex_ = 0;

    Window& mWindow_;

    GraphicsContextDesktop& mGraphicsContextDesktop_;

    std::array<std::unique_ptr<BaseScene>, 2> mSceneBuffer_;

    bool mFramebufferResized_ = false;

    std::chrono::steady_clock::time_point mLastTime_;
};

} // namespace clay

#endif // CLAY_PLATFORM_DESKTOP