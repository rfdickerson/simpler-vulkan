#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

// 3D camera for the diorama-style view
class Camera {
public:
    // Camera positioning
    glm::vec3 position;
    glm::vec3 target;
    glm::vec3 up;
    
    // Projection parameters
    float fov;          // Field of view in degrees
    float aspectRatio;
    float nearPlane;
    float farPlane;
    
    // Tilt-shift style parameters
    float tiltAngle;    // Angle of the camera looking down (in degrees)
    float orbitRadius;  // Distance from target
    float orbitAngle;   // Rotation around target (in degrees)
    
    Camera()
        : position(0.0f, 10.0f, 10.0f)
        , target(0.0f, 0.0f, 0.0f)
        , up(0.0f, 1.0f, 0.0f)
        , fov(45.0f)
        , aspectRatio(16.0f / 9.0f)
        , nearPlane(0.1f)
        , farPlane(1000.0f)
        , tiltAngle(60.0f)
        , orbitRadius(15.0f)
        , orbitAngle(45.0f)
    {
        updatePosition();
    }
    
    // Update camera position based on orbit parameters
    void updatePosition() {
        float tiltRad = glm::radians(tiltAngle);
        float orbitRad = glm::radians(orbitAngle);
        
        position.x = target.x + orbitRadius * std::cos(tiltRad) * std::cos(orbitRad);
        position.y = target.y + orbitRadius * std::sin(tiltRad);
        position.z = target.z + orbitRadius * std::cos(tiltRad) * std::sin(orbitRad);
    }
    
    // Get view matrix
    glm::mat4 getViewMatrix() const {
        return glm::lookAt(position, target, up);
    }
    
    // Get projection matrix
    glm::mat4 getProjectionMatrix() const {
        return glm::perspective(glm::radians(fov), aspectRatio, nearPlane, farPlane);
    }
    
    // Get combined view-projection matrix
    glm::mat4 getViewProjectionMatrix() const {
        return getProjectionMatrix() * getViewMatrix();
    }
    
    // Move the camera target (panning)
    void pan(float dx, float dz) {
        target.x += dx;
        target.z += dz;
        updatePosition();
    }
    
    // Zoom in/out (adjust orbit radius)
    void zoom(float delta) {
        orbitRadius = glm::clamp(orbitRadius + delta, 5.0f, 100.0f);
        updatePosition();
    }
    
    // Rotate around target
    void rotate(float angleDelta) {
        orbitAngle += angleDelta;
        // Normalize to [0, 360)
        while (orbitAngle >= 360.0f) orbitAngle -= 360.0f;
        while (orbitAngle < 0.0f) orbitAngle += 360.0f;
        updatePosition();
    }
    
    // Adjust tilt angle (look more/less down)
    void tilt(float angleDelta) {
        tiltAngle = glm::clamp(tiltAngle + angleDelta, 30.0f, 89.0f);
        updatePosition();
    }
    
    // Set aspect ratio (call when window resizes)
    void setAspectRatio(float aspect) {
        aspectRatio = aspect;
    }
    
    // Focus on a specific world position
    void focusOn(const glm::vec3& worldPos) {
        target = worldPos;
        updatePosition();
    }
};

