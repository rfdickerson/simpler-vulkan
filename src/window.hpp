#pragma once

#include <glfw/glfw3.h>

struct Window {
    GLFWwindow* window{};
    int         width;
    int         height;

    void InitGLFW();
    void CreateWindow(int width, int height, const char* title);
    void        onResize(int width, int height);
    static void pollEvents();
    bool        shouldClose() const;
    void        cleanup() const;
};