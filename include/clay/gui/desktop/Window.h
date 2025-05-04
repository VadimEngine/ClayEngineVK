#pragma once

#ifdef CLAY_PLATFORM_DESKTOP

// standard lib
#include <utility>
// third party
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>
// clay
#include "clay/application/desktop/InputHandlerDesktop.h"

namespace clay {


class Window {
public:
    Window();

    ~Window();

    void update(float dt);

    void render();

    std::pair<int, int> getDimensions() const;

    GLFWwindow* getGLFWWindow() const;

    bool isRunning() const;

    VkSurfaceKHR createSurface(VkInstance& instance);

    InputHandlerDesktop* getInputHandler();

private:
    /** Default Screen Width */
    static constexpr unsigned int SCR_WIDTH = 800;
    /** Default Screen Height */
    static constexpr unsigned int SCR_HEIGHT = 600;
    /** GLFW window this window wraps */
    GLFWwindow* mpGLFWWindow_ = nullptr;
    /** Input handler listening to inputs on this window*/
    InputHandlerDesktop mInputHandler_;

    /** GLFW swap interval for VSync */
    unsigned int mSwapInterval_ = 1;
};

} // namespace clay

#endif // CLAY_PLATFORM_DESKTOP