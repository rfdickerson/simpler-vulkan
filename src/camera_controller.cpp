#include "camera_controller.hpp"

#include <glm/glm.hpp>
#include <iostream>

#include "swapchain.hpp"
#include "terrain_example.hpp"
#include "hex_coord.hpp"

void CameraController::update(Window& window, Camera& camera, float deltaTime) {
    handlePan(window, camera);
    handleScroll(window, camera);
    handleKeyboard(window, camera, deltaTime);
}

void CameraController::handleClick(Window& window, const Swapchain& swapchain, TerrainExample& terrainExample) {
    double clickX = 0.0;
    double clickY = 0.0;
    if (!window.consumeLeftMouseClick(clickX, clickY)) {
        return;
    }

    Camera& camera = terrainExample.getCamera();
    glm::vec3 worldPos = camera.unprojectToPlane(
        static_cast<float>(clickX),
        static_cast<float>(clickY),
        static_cast<float>(swapchain.extent.width),
        static_cast<float>(swapchain.extent.height),
        0.0f);

    float hexSize = terrainExample.getHexSize();
    HexCoord hex = worldToHex(worldPos, hexSize);

    std::cout << "Clicked hex tile: (" << hex.q << ", " << hex.r << ")" << std::endl;
}

void CameraController::handlePan(Window& window, Camera& camera) {
    float panDX = 0.0f;
    float panDY = 0.0f;
    if (!window.consumeCameraPanDelta(panDX, panDY)) {
        return;
    }

    glm::vec3 viewDir = glm::normalize(camera.target - camera.position);
    glm::vec2 forwardXZ = glm::normalize(glm::vec2(viewDir.x, viewDir.z));
    glm::vec2 rightXZ = glm::vec2(forwardXZ.y, -forwardXZ.x);

    float sensitivity = 0.0025f * camera.orbitRadius;
    float dxWorld = (-panDX) * sensitivity * rightXZ.x + (-panDY) * sensitivity * forwardXZ.x;
    float dzWorld = (-panDX) * sensitivity * rightXZ.y + (-panDY) * sensitivity * forwardXZ.y;
    camera.pan(dxWorld, dzWorld);
}

void CameraController::handleScroll(Window& window, Camera& camera) {
    float scrollX = 0.0f;
    float scrollY = 0.0f;
    if (!window.consumeScrollDelta(scrollX, scrollY)) {
        return;
    }

    static_cast<void>(scrollX);

    bool altDown = window.isKeyDown(GLFW_KEY_LEFT_ALT) || window.isKeyDown(GLFW_KEY_RIGHT_ALT);
    if (altDown) {
        float rotateSensitivityDeg = 5.0f;
        camera.rotate(-scrollY * rotateSensitivityDeg);
    } else {
        float zoomSensitivity = 1.0f;
        camera.zoom(-scrollY * zoomSensitivity);
    }
}

void CameraController::handleKeyboard(Window& window, Camera& camera, float deltaTime) {
    int moveForward = window.isKeyDown(GLFW_KEY_W) ? 1 : 0;
    int moveBackward = window.isKeyDown(GLFW_KEY_S) ? 1 : 0;
    int moveLeft = window.isKeyDown(GLFW_KEY_A) ? 1 : 0;
    int moveRight = window.isKeyDown(GLFW_KEY_D) ? 1 : 0;

    int forwardAxis = moveForward - moveBackward;
    int strafeAxis = moveRight - moveLeft;

    if (forwardAxis == 0 && strafeAxis == 0) {
        return;
    }

    glm::vec3 viewDir = glm::normalize(camera.target - camera.position);
    glm::vec2 forwardXZ = glm::normalize(glm::vec2(viewDir.x, viewDir.z));
    glm::vec2 rightXZ = glm::vec2(forwardXZ.y, -forwardXZ.x);

    float baseSpeed = 5.0f;
    float speed = baseSpeed * deltaTime * glm::max(camera.orbitRadius * 0.1f, 1.0f);

    float dxWorld = (strafeAxis * rightXZ.x + forwardAxis * forwardXZ.x) * speed;
    float dzWorld = (strafeAxis * rightXZ.y + forwardAxis * forwardXZ.y) * speed;
    camera.pan(dxWorld, dzWorld);
}
