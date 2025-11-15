#pragma once

#include "camera.hpp"
#include "window.hpp"

struct Swapchain;
class TerrainExample;

class CameraController {
public:
    void update(Window& window, Camera& camera, float deltaTime);
    void handleClick(Window& window, const Swapchain& swapchain, TerrainExample& terrainExample);

private:
    void handlePan(Window& window, Camera& camera);
    void handleScroll(Window& window, Camera& camera);
    void handleKeyboard(Window& window, Camera& camera, float deltaTime);
};
