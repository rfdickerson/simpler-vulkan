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

    // scroll callback
    glfwSetScrollCallback(window, [](GLFWwindow* window, double xoffset, double yoffset) {
        auto app = static_cast<Window*>(glfwGetWindowUserPointer(window));
        app->onScroll(xoffset, yoffset);
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
    if (button == GLFW_MOUSE_BUTTON_MIDDLE) {
        if (action == GLFW_PRESS) {
            middleMousePressed = true;
            glfwGetCursorPos(window, &lastMouseX, &lastMouseY);
        } else if (action == GLFW_RELEASE) {
            middleMousePressed = false;
        }
    } else if (button == GLFW_MOUSE_BUTTON_LEFT) {
        if (action == GLFW_PRESS) {
            leftMouseClicked = true;
            glfwGetCursorPos(window, &clickX, &clickY);
        }
    }
}

void Window::onMouseMove(double xpos, double ypos) {
    if (middleMousePressed) {
        double deltaX = xpos - lastMouseX;
        double deltaY = ypos - lastMouseY;
        
        cameraOffsetX += static_cast<float>(deltaX);
        cameraOffsetY += static_cast<float>(deltaY);
        
        lastMouseX = xpos;
        lastMouseY = ypos;
    }
}

void Window::onScroll(double xoffset, double yoffset) {
    scrollX += static_cast<float>(xoffset);
    scrollY += static_cast<float>(yoffset);
}

bool Window::consumeCameraPanDelta(float& outDx, float& outDy) {
	outDx = cameraOffsetX;
	outDy = cameraOffsetY;
	cameraOffsetX = 0.0f;
	cameraOffsetY = 0.0f;
	return (outDx != 0.0f || outDy != 0.0f);
}

bool Window::consumeLeftMouseClick(double& outX, double& outY) {
	if (leftMouseClicked) {
		outX = clickX;
		outY = clickY;
		leftMouseClicked = false;
		return true;
	}
	return false;
}

bool Window::consumeScrollDelta(float& outX, float& outY) {
    outX = scrollX;
    outY = scrollY;
    scrollX = 0.0f;
    scrollY = 0.0f;
    return (outX != 0.0f || outY != 0.0f);
}

bool Window::isKeyDown(int key) const {
    return glfwGetKey(window, key) == GLFW_PRESS;
}