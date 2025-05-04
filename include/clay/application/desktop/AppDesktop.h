#pragma once

#ifdef CLAY_PLATFORM_DESKTOP

// standard lib
#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <cstdlib>
#include <vector>
#include <set>
#include <array>
#include <chrono>
// third party
// move all glfw to window.h?
#define GLFW_INCLUDE_VULKAN 
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>
#define GLM_ENABLE_EXPERIMENTAL
// clay
#include "clay/gui/desktop/Window.h"
#include "clay/utils/desktop/UtilsDesktop.h"
#include "clay/application/common/BaseScene.h"
#include "clay/graphics/desktop/GraphicsContextDesktop.h"
#include "clay/application/common/Resources.h"
#include "clay/application/common/IApp.h"

namespace clay {

class AppDesktop : public clay::IApp {
public:
    AppDesktop();

    virtual ~AppDesktop();

    virtual void loadResources() = 0;

    void setScene(BaseScene* pScene);

    void run();

    Window& getWindow();

    Resources& getResources() override;

    AudioManager& getAudioManager() override;

    GraphicsContextDesktop& getGraphicsContext();

    void quit();

protected:
    void initVulkan();

    void initImgui();

    void mainLoop();
    
    void cleanup();

    void createCommandBuffers();

    void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex);

    void drawFrame();

    Window mWindow_;

    GraphicsContextDesktop mGraphicsContext_;

    Resources mResources_;

    std::array<BaseScene*, 2> mSceneBuffer_;

    VkDescriptorPool mImguiDescriptorPool_;

    std::vector<VkCommandBuffer> mCommandBuffers_;
    bool mFramebufferResized_ = false; // TODO move to window?

    std::chrono::steady_clock::time_point mLastTime_;

    AudioManager mAudioManger_;
};

} // namespace clay

#endif // CLAY_PLATFORM_DESKTOP