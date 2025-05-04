#include <stdexcept>
// class
#include "clay/gui/Window.h"

void mouse_callback(GLFWwindow* window, double xPos, double yPos) {
   Window* windowWrapper = static_cast<Window*>(glfwGetWindowUserPointer(window));
   InputHandler& inputHandler = *(InputHandler*)windowWrapper->getInputHandler();
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
   InputHandler& inputHandler = *(InputHandler*)windowWrapper->getInputHandler();

   double xpos, ypos;
   glfwGetCursorPos(window, &xpos, &ypos);

   if (button == GLFW_MOUSE_BUTTON_LEFT) {
       if (action == GLFW_PRESS) {
           inputHandler.onMousePress(
               InputHandler::MouseEvent::Button::LEFT
           );
       } else if (action == GLFW_RELEASE) {
           inputHandler.onMouseRelease(
               InputHandler::MouseEvent::Button::LEFT
           );
       }
   } else if (button == GLFW_MOUSE_BUTTON_RIGHT) {
       if (action == GLFW_PRESS) {
           inputHandler.onMousePress(
               InputHandler::MouseEvent::Button::RIGHT
           );
       } else if (action == GLFW_RELEASE) {
           inputHandler.onMouseRelease(
               InputHandler::MouseEvent::Button::RIGHT
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
   InputHandler& inputHandler = *(InputHandler*)windowWrapper->getInputHandler();

   inputHandler.onMouseWheel(yOffset);
}

void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
   Window* windowWrapper = static_cast<Window*>(glfwGetWindowUserPointer(window));
   InputHandler& inputHandler = *(InputHandler*)windowWrapper->getInputHandler();

   if (action == GLFW_PRESS) {
       inputHandler.onKeyPressed(key);
   } else if (action == GLFW_RELEASE) {
       inputHandler.onKeyReleased(key);
   }
   // GLFW_REPEAT
}

void framebufferResizeCallback(GLFWwindow* window, int width, int height) {
    //auto app = reinterpret_cast<HelloTriangleApplication*>(glfwGetWindowUserPointer(window));
    //app->framebufferResized = true;
}   

Window::Window() {
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    mpGLFWWindow_ = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Vulkan", nullptr, nullptr);
    glfwSetWindowUserPointer(mpGLFWWindow_, this);

    glfwSetFramebufferSizeCallback(mpGLFWWindow_, framebufferResizeCallback);
    // key callback
    glfwSetKeyCallback(mpGLFWWindow_, keyCallback);
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

void Window::render() {

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

VkSurfaceKHR Window::createSurface(VkInstance& instance) {
    VkSurfaceKHR surface;

    // Create the surface using GLFW and Vulkan instance
    if (glfwCreateWindowSurface(instance, mpGLFWWindow_, nullptr, &surface) != VK_SUCCESS) {
        throw std::runtime_error("failed to create window surface!");
    }

    // Return the surface
    return surface;
}

InputHandler* Window::getInputHandler() {
    return &mInputHandler_;
}
