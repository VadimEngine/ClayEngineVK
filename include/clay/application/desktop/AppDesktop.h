#pragma once

#ifdef CLAY_PLATFORM_DESKTOP
// standard lib
#include <stdexcept>
#include <cstdlib>
#include <array>
#include <chrono>
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

    bool tempVSyncFlag = false;
    bool tempVSyncValue = false;


protected:
    void recordCommandBuffer(vk::CommandBuffer commandBuffer, uint32_t imageIndex);
    
    Window& mWindow_;

    GraphicsContextDesktop& mGraphicsContextDesktop_;

    std::array<std::unique_ptr<BaseScene>, 2> mSceneBuffer_;

    bool mFramebufferResized_ = false;

    std::chrono::steady_clock::time_point mLastTime_;
};

} // namespace clay

#endif // CLAY_PLATFORM_DESKTOP