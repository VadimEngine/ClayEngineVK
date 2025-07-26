#pragma once

#ifdef CLAY_PLATFORM_DESKTOP

// standard lib
#include <utility>
#include <optional>
// third party
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.hpp>
// clay
#include <clay/application/desktop/InputHandlerDesktop.h>

namespace clay {

class Window {
public:
    enum class WindowEvent {
        RESIZED, CLOSED, FOCUS_GAINED, FOCUS_LOST
    };

    Window(int width = DEFAULT_WIDTH, int height = DEFAULT_HEIGHT);

    ~Window();

    void update(float dt);

    std::pair<int, int> getDimensions() const;

    GLFWwindow* getGLFWWindow() const;

    bool isRunning() const;

    vk::SurfaceKHR createSurface(vk::Instance& instance);

    void pushEvent(WindowEvent evt);

    std::optional<WindowEvent> pollEvent();

    InputHandlerDesktop& getInputHandler();

private:
    /** Default Screen Width */
    static constexpr unsigned int DEFAULT_WIDTH = 800;
    /** Default Screen Height */
    static constexpr unsigned int DEFAULT_HEIGHT = 600;
    /** GLFW window this window wraps */
    GLFWwindow* mpGLFWWindow_ = nullptr;
    /** Input handler listening to Mouse/Keyboard inputs on this window*/
    InputHandlerDesktop mInputHandler_;
    /** Queue to hold window events such as resize, close, focus lost/gain */
    std::queue<WindowEvent> mWindowEventQueue_;
};

} // namespace clay

#endif // CLAY_PLATFORM_DESKTOP