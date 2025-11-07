#pragma once

#include <glfw/glfw3.h>

struct Window {
    GLFWwindow* window{};
    int         width;
    int         height;

    void InitGLFW();
};