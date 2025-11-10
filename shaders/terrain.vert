#version 450

// Vertex attributes
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUV;
layout(location = 3) in vec2 inHexCoord;

// Push constants (per-draw data)
layout(push_constant) uniform PushConstants {
    mat4 viewProj;
    vec3 cameraPos;
    float time;
} pc;

// Output to fragment shader
layout(location = 0) out vec3 fragWorldPos;
layout(location = 1) out vec3 fragNormal;
layout(location = 2) out vec2 fragUV;
layout(location = 3) out vec2 fragHexCoord;

void main() {
    // Transform to world space (already in world space, so just pass through)
    vec4 worldPos = vec4(inPosition, 1.0);
    
    // Output position for fragment shader
    fragWorldPos = worldPos.xyz;
    fragNormal = inNormal;
    fragUV = inUV;
    fragHexCoord = inHexCoord;
    
    // Transform to clip space
    gl_Position = pc.viewProj * worldPos;
}

