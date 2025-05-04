#pragma once
// standard lib
#include <utility>
// third party
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>
// clay
#include "clay/application/InputHandler.h"

class Window {
public:
    Window();

    ~Window();

    void update(float dt);

    void render();

    std::pair<int, int> getDimensions() const;

    GLFWwindow *getGLFWWindow() const;

    bool isRunning() const;

    VkSurfaceKHR createSurface(VkInstance& instance);

    InputHandler* getInputHandler();

private:
    /** Default Screen Width */
    static constexpr unsigned int SCR_WIDTH = 800;
    /** Default Screen Height */
    static constexpr unsigned int SCR_HEIGHT = 600;
    /** GLFW window this window wraps */
    GLFWwindow* mpGLFWWindow_ = nullptr;
    /** Input handler listening to inputs on this window*/
    InputHandler mInputHandler_;

    /** GLFW swap interval for VSync */
    unsigned int mSwapInterval_ = 1;
};