#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>

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
        glm::mat4 proj = glm::perspective(glm::radians(fov), aspectRatio, nearPlane, farPlane);
        // Vulkan's clip space has inverted Y compared to GL's default. Flip it here.
        proj[1][1] *= -1.0f;
        return proj;
    }
    
    // Get combined view-projection matrix
    glm::mat4 getViewProjectionMatrix() const {
        return getProjectionMatrix() * getViewMatrix();
    }
    
    // Move the camera target (panning)
    void pan(const float dx, const float dz) {
        target.x += dx;
        target.z += dz;
        updatePosition();
    }

    // Zoom in/out (adjust orbit radius)
    void zoom(const float delta) {
        constexpr float MIN_ORBIT_RADIUS = 5.0f;
        constexpr float MAX_ORBIT_RADIUS = 100.0f;
        orbitRadius = glm::clamp(orbitRadius + delta, MIN_ORBIT_RADIUS, MAX_ORBIT_RADIUS);
        updatePosition();
    }

    // Rotate around target
    void rotate(const float angleDelta) {
        orbitAngle += angleDelta;
        // Normalize to [0, 360)
        while (orbitAngle >= 360.0f) orbitAngle -= 360.0f;
        while (orbitAngle < 0.0f) orbitAngle += 360.0f;
        updatePosition();
    }

    // Adjust tilt angle (look more/less down)
    void tilt(const float angleDelta) {
        constexpr float MIN_TILT_ANGLE = 30.0f;
        constexpr float MAX_TILT_ANGLE = 89.0f;
        tiltAngle = glm::clamp(tiltAngle + angleDelta, MIN_TILT_ANGLE, MAX_TILT_ANGLE);
        updatePosition();
    }

    // Set aspect ratio (call when window resizes)
    void setAspectRatio(const float aspect) {
        aspectRatio = aspect;
    }

    // Reset camera to defaults (keeps current aspect and fov)
    void reset() {
        target = glm::vec3(0.0f, 0.0f, 0.0f);
        tiltAngle = 60.0f;
        orbitRadius = 15.0f;
        orbitAngle = 45.0f;
        updatePosition();
    }

    // Focus on a specific world position
    void focusOn(const glm::vec3& worldPos) {
        target = worldPos;
        updatePosition();
    }
    
    // Unproject screen coordinates to world position on a plane
    // screenX, screenY: pixel coordinates (0 to width, 0 to height)
    // screenWidth, screenHeight: window dimensions
    // planeY: Y coordinate of the plane to intersect (typically 0 for ground)
    glm::vec3 unprojectToPlane(const float screenX, const float screenY,
                              const float screenWidth, const float screenHeight,
                              const float planeY = 0.0f) const {
        // Convert screen coordinates to normalized device coordinates (NDC)
        // NDC: x in [-1, 1], y in [-1, 1], with origin at center
        // Screen: x in [0, width], y in [0, height], with origin at top-left
        float ndcX = (2.0f * screenX / screenWidth) - 1.0f;
        float ndcY = 1.0f - (2.0f * screenY / screenHeight); // Flip Y axis
        
        // Create a point on the near plane in clip space (after perspective divide)
        // Use z = 0.0 for near plane in NDC (which maps to -near in view space)
        glm::vec4 clipPos(ndcX, ndcY, 0.0f, 1.0f);
        
        // Transform to view space
        glm::mat4 invProj = glm::inverse(getProjectionMatrix());
        glm::vec4 viewPos = invProj * clipPos;
        viewPos /= viewPos.w; // Perspective divide
        
        // Transform to world space
        glm::mat4 invView = glm::inverse(getViewMatrix());
        glm::vec4 worldPos4 = invView * viewPos;
        glm::vec3 worldPosNear = glm::vec3(worldPos4);
        
        // Create a second point on the far plane to get ray direction
        glm::vec4 clipPosFar(ndcX, ndcY, 1.0f, 1.0f);
        glm::vec4 viewPosFar = invProj * clipPosFar;
        viewPosFar /= viewPosFar.w;
        glm::vec4 worldPosFar4 = invView * viewPosFar;
        glm::vec3 worldPosFar = glm::vec3(worldPosFar4);
        
        // Calculate ray direction
        glm::vec3 rayDir = glm::normalize(worldPosFar - worldPosNear);
        
        // Calculate intersection with plane at y = planeY
        // Ray equation: P = origin + t * direction
        // Plane equation: y = planeY
        // Solve: worldPosNear.y + t * rayDir.y = planeY
        // t = (planeY - worldPosNear.y) / rayDir.y
        if (std::abs(rayDir.y) < 0.0001f) {
            // Ray is nearly parallel to plane, return a default position
            return glm::vec3(0.0f, planeY, 0.0f);
        }
        
        float t = (planeY - worldPosNear.y) / rayDir.y;
        glm::vec3 worldPos = worldPosNear + t * rayDir;
        
        return worldPos;
    }
};

