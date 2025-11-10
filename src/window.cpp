#include "window.hpp"


void Window::InitGLFW() {
    glfwInit();
}

void Window::CreateWindow(int width, int height, const char* title) {
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    window       = glfwCreateWindow(width, height, title, nullptr, nullptr);
    this->width  = width;
    this->height = height;

    // set up the callbacks
    glfwSetWindowUserPointer(window, this);

    // resize callback
    glfwSetFramebufferSizeCallback(window, [](GLFWwindow* window, int width, int height) {
        auto app = static_cast<Window*>(glfwGetWindowUserPointer(window));
        app->onResize(width, height);
    });

    // mouse button callback
    glfwSetMouseButtonCallback(window, [](GLFWwindow* window, int button, int action, int mods) {
        auto app = static_cast<Window*>(glfwGetWindowUserPointer(window));
        app->onMouseButton(button, action, mods);
    });

    // cursor position callback
    glfwSetCursorPosCallback(window, [](GLFWwindow* window, double xpos, double ypos) {
        auto app = static_cast<Window*>(glfwGetWindowUserPointer(window));
        app->onMouseMove(xpos, ypos);
    });
}

void Window::onResize(int width, int height) {
    // handle resize
    // std::cout << "Window resized to " << width << "x" << height << std::endl;
    this->width  = width;
    this->height = height;
}

void Window::pollEvents() {
    glfwPollEvents();
}

bool Window::shouldClose() const {
    return glfwWindowShouldClose(window);
}

void Window::cleanup() const {
    if (window)
        glfwDestroyWindow(window);
    glfwTerminate();
}

void Window::onMouseButton(int button, int action, int mods) {
    if (button == GLFW_MOUSE_BUTTON_RIGHT) {
        if (action == GLFW_PRESS) {
            rightMousePressed = true;
            glfwGetCursorPos(window, &lastMouseX, &lastMouseY);
        } else if (action == GLFW_RELEASE) {
            rightMousePressed = false;
        }
    }
}

void Window::onMouseMove(double xpos, double ypos) {
    if (rightMousePressed) {
        // Calculate mouse delta
        double deltaX = xpos - lastMouseX;
        double deltaY = ypos - lastMouseY;
        
        // Update camera offset
        cameraOffsetX += static_cast<float>(deltaX);
        cameraOffsetY += static_cast<float>(deltaY);
        
        // Update last mouse position
        lastMouseX = xpos;
        lastMouseY = ypos;
    }
}