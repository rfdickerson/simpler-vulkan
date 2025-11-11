#version 450

// Per-vertex attributes (box geometry)
layout(location = 0) in vec3 inPosition;

// Per-instance attributes
layout(location = 1) in vec3 instancePos;
layout(location = 2) in float instanceRotation;

// Push constants
layout(push_constant) uniform PushConstants {
    mat4 viewProj;
} pc;

// Output to fragment shader
layout(location = 0) out vec3 fragWorldPos;
layout(location = 1) out vec3 fragNormal;

void main() {
    // Rotate around Y-axis
    float cosRot = cos(instanceRotation);
    float sinRot = sin(instanceRotation);
    
    mat3 rotationMatrix = mat3(
        cosRot, 0.0, sinRot,
        0.0, 1.0, 0.0,
        -sinRot, 0.0, cosRot
    );
    
    // Apply rotation and translation
    vec3 rotatedPos = rotationMatrix * inPosition;
    vec3 worldPos = rotatedPos + instancePos;
    
    // Calculate normal (simple per-face normal approximation)
    // For a box, we can derive normal from the vertex position relative to center
    vec3 normal = normalize(rotationMatrix * sign(inPosition));
    
    // Output
    fragWorldPos = worldPos;
    fragNormal = normal;
    
    // Transform to clip space
    gl_Position = pc.viewProj * vec4(worldPos, 1.0);
}

