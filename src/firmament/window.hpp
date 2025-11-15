#pragma once

#include <glfw/glfw3.h>

namespace firmament {

struct Window {
    GLFWwindow* window{};
    int         width;
    int         height;

    // Mouse state for camera panning
    bool middleMousePressed = false;
    double lastMouseX = 0.0;
    double lastMouseY = 0.0;
    float cameraOffsetX = 0.0f;
    float cameraOffsetY = 0.0f;

    // Scroll state
    float scrollX = 0.0f;
    float scrollY = 0.0f;
    
    // Mouse state for left click
    bool leftMouseClicked = false;
    double clickX = 0.0;
    double clickY = 0.0;

    void InitGLFW();
    void CreateWindow(int width, int height, const char* title);
    void        onResize(int width, int height);
    static void pollEvents();
    bool        shouldClose() const;
    void        cleanup() const;
    
    void onMouseButton(int button, int action, int mods);
    void onMouseMove(double xpos, double ypos);
    void onScroll(double xoffset, double yoffset);
	bool consumeCameraPanDelta(float& outDx, float& outDy);
	bool consumeLeftMouseClick(double& outX, double& outY);
    bool consumeScrollDelta(float& outX, float& outY);
    bool isKeyDown(int key) const;
};
} // namespace firmament
