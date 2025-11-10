#pragma once

#include <glfw/glfw3.h>

struct Window {
    GLFWwindow* window{};
    int         width;
    int         height;

    // Mouse state for camera panning
    bool rightMousePressed = false;
    double lastMouseX = 0.0;
    double lastMouseY = 0.0;
    float cameraOffsetX = 0.0f;
    float cameraOffsetY = 0.0f;

    void InitGLFW();
    void CreateWindow(int width, int height, const char* title);
    void        onResize(int width, int height);
    static void pollEvents();
    bool        shouldClose() const;
    void        cleanup() const;
    
    void onMouseButton(int button, int action, int mods);
    void onMouseMove(double xpos, double ypos);
	bool consumeCameraPanDelta(float& outDx, float& outDy);
};