#ifdef CLAY_PLATFORM_DESKTOP

#include <stdexcept>
// class
#include "clay/gui/desktop/Window.h"

namespace clay {


void mouse_callback(GLFWwindow* window, double xPos, double yPos) {
   Window* windowWrapper = static_cast<Window*>(glfwGetWindowUserPointer(window));
   InputHandlerDesktop& inputHandler = (InputHandlerDesktop&)windowWrapper->getInputHandler();
   inputHandler.onMouseMove(static_cast<int>(xPos), static_cast<int>(yPos));
   //get width and height
   int winHeight;
   int winWidth;

   glfwGetWindowSize(window, &winWidth, &winHeight);

   GLfloat mx = ((2.0f * xPos / winWidth) - 1.0f);
   GLfloat my = (1.0f - (2.0f * yPos / winHeight));
   //((Window*)glfwGetWindowUserPointer(window))->getHandler()->mouseCoords = glm::vec2(mx, my);
   //((Window*)glfwGetWindowUserPointer(window))->getHandler()->setMousePosition(glm::vec2(mx, my));
}

void mouse_button_callback(GLFWwindow* window, int button, int action, int mods) {
   Window* windowWrapper = static_cast<Window*>(glfwGetWindowUserPointer(window));
   InputHandlerDesktop& inputHandler = (InputHandlerDesktop&)windowWrapper->getInputHandler();

   double xpos, ypos;
   glfwGetCursorPos(window, &xpos, &ypos);

   if (button == GLFW_MOUSE_BUTTON_LEFT) {
       if (action == GLFW_PRESS) {
           inputHandler.onMousePress(
               InputHandlerDesktop::MouseEvent::Button::LEFT
           );
       } else if (action == GLFW_RELEASE) {
           inputHandler.onMouseRelease(
               InputHandlerDesktop::MouseEvent::Button::LEFT
           );
       }
   } else if (button == GLFW_MOUSE_BUTTON_RIGHT) {
       if (action == GLFW_PRESS) {
           inputHandler.onMousePress(
               InputHandlerDesktop::MouseEvent::Button::RIGHT
           );
       } else if (action == GLFW_RELEASE) {
           inputHandler.onMouseRelease(
               InputHandlerDesktop::MouseEvent::Button::RIGHT
           );
       }
   } else if (button == GLFW_MOUSE_BUTTON_MIDDLE) {
       if (action == GLFW_PRESS) {
           inputHandler.onMousePress(
               InputHandlerDesktop::MouseEvent::Button::MIDDLE
           );
       } else if (action == GLFW_RELEASE) {
           inputHandler.onMouseRelease(
               InputHandlerDesktop::MouseEvent::Button::MIDDLE
           );
       }
   }

   /**
   The mods parameter is a combination of one or more of the following constants:

   GLFW_MOD_SHIFT: Set if one of the Shift keys is held down.
   GLFW_MOD_CONTROL: Set if one of the Control keys is held down.
   GLFW_MOD_ALT: Set if one of the Alt keys is held down.
   GLFW_MOD_SUPER: Set if one of the Super keys is held down.
    */
}

void mouseWheelCallback(GLFWwindow* window, double xOffset, double yOffset) {
   Window* windowWrapper = static_cast<Window*>(glfwGetWindowUserPointer(window));
   InputHandlerDesktop& inputHandler = (InputHandlerDesktop&)windowWrapper->getInputHandler();

   inputHandler.onMouseWheel(yOffset);
}

void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
   Window* windowWrapper = static_cast<Window*>(glfwGetWindowUserPointer(window));
   InputHandlerDesktop& inputHandler = (InputHandlerDesktop&)windowWrapper->getInputHandler();

   if (action == GLFW_PRESS || action == GLFW_REPEAT) {
       // REPEAT (auto-repeat while held) is queued like a press so held editing keys
       // (backspace, arrows, delete) repeat in text fields. It re-sets an already-set
       // key bit (harmless) and only the key-EVENT queue grows, which gameplay ignores.
       inputHandler.onKeyPressed(key);
   } else if (action == GLFW_RELEASE) {
       inputHandler.onKeyReleased(key);
   }
}

void charCallback(GLFWwindow* window, unsigned int codepoint) {
   Window* windowWrapper = static_cast<Window*>(glfwGetWindowUserPointer(window));
   InputHandlerDesktop& inputHandler = (InputHandlerDesktop&)windowWrapper->getInputHandler();
   inputHandler.onCharInput(codepoint);
}

void framebufferResizeCallback(GLFWwindow* glfwWindow, int width, int height) {
    auto window = reinterpret_cast<Window*>(glfwGetWindowUserPointer(glfwWindow));
    window->pushEvent(Window::WindowEvent::RESIZED);
}   

Window::Window(int width, int height, const char* name) {
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    // Dev: CLAY_WINDOW_HIDDEN=1 runs without showing a window (frames still render, so
    // captures work) - for scripted runs that must not pop up over what the user is doing.
    if (const char* hidden = std::getenv("CLAY_WINDOW_HIDDEN"); hidden && hidden[0] == '1') {
        glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
        // No title bar or border either: Windows limits a decorated window to the size of the
        // screen, and captures are often asked for bigger than that (a phone-sized portrait).
        glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);
    }

    mpGLFWWindow_ = glfwCreateWindow(width, height, name, nullptr, nullptr);
    glfwSetWindowUserPointer(mpGLFWWindow_, this);

    glfwSetFramebufferSizeCallback(mpGLFWWindow_, framebufferResizeCallback);
    // key callback
    glfwSetKeyCallback(mpGLFWWindow_, keyCallback);
    glfwSetCharCallback(mpGLFWWindow_, charCallback);
    // mouse callbacks
    glfwSetCursorPosCallback(mpGLFWWindow_, mouse_callback);
    glfwSetMouseButtonCallback(mpGLFWWindow_, mouse_button_callback);
    glfwSetScrollCallback(mpGLFWWindow_, mouseWheelCallback);
}

Window::~Window() {
    glfwDestroyWindow(mpGLFWWindow_);
    glfwTerminate();
}

void Window::update(float dt) {
    glfwPollEvents();
}

std::pair<int, int> Window::getDimensions() const {
    std::pair<int, int> dim;
    glfwGetFramebufferSize(mpGLFWWindow_, &dim.first, &dim.second);
    
    return dim;
}

GLFWwindow* Window::getGLFWWindow() const {
   return mpGLFWWindow_;
}

bool Window::isRunning() const {
    return !glfwWindowShouldClose(mpGLFWWindow_);
}

vk::SurfaceKHR Window::createSurface(vk::Instance& instance) {
    VkSurfaceKHR surface;

    // Create the surface using GLFW and Vulkan instance
    if (glfwCreateWindowSurface(instance, mpGLFWWindow_, nullptr, &surface) != VK_SUCCESS) {
        throw std::runtime_error("failed to create window surface!");
    }

    return (vk::SurfaceKHR)surface;
}

void Window::pushEvent(WindowEvent evt) {
    mWindowEventQueue_.push(evt);
}

std::optional<Window::WindowEvent> Window::pollEvent() {
    if (mWindowEventQueue_.empty()) {
        return std::nullopt;
    }

    WindowEvent outEvent = mWindowEventQueue_.front();
    mWindowEventQueue_.pop();
    return outEvent;
}

InputHandlerDesktop& Window::getInputHandler() {
    return mInputHandler_;
}

} // namespace clay

#endif // CLAY_PLATFORM_DESKTOP