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