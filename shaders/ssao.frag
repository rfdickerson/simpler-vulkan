#version 450

layout(location = 0) in vec2 vUV;
layout(location = 0) out float outOcclusion;

layout(set = 0, binding = 0) uniform sampler2D uDepth;

layout(push_constant) uniform Push {
    float radius;        // screen-space radius
    float bias;
    float intensity;
    float _pad0;
    mat4 invProj;
} pc;

// Poisson disk samples for better distribution
const vec2 poisson[16] = vec2[](
    vec2(-0.94201624, -0.39906216), vec2(0.94558609, -0.76890725),
    vec2(-0.094184101, -0.92938870), vec2(0.34495938, 0.29387760),
    vec2(-0.91588581, 0.45771432), vec2(-0.81544232, -0.87912464),
    vec2(-0.38277543, 0.27676845), vec2(0.97484398, 0.75648379),
    vec2(0.44323325, -0.97511554), vec2(0.53742981, -0.47373420),
    vec2(-0.26496911, -0.41893023), vec2(0.79197514, 0.19090188),
    vec2(-0.24188840, 0.99706507), vec2(-0.81409955, 0.91437590),
    vec2(0.19984126, 0.78641367), vec2(0.14383161, -0.14100790)
);

// Simple hash for per-pixel randomization
float hash(vec2 p) {
    float h = dot(p, vec2(127.1, 311.7));
    return fract(sin(h) * 43758.5453123);
}

// Reconstruct view-space position from depth
vec3 getViewPosition(vec2 uv, float depth) {
    vec4 clipPos = vec4(uv * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
    vec4 viewPos = pc.invProj * clipPos;
    return viewPos.xyz / viewPos.w;
}

void main() {
    vec2 texelSize = 1.0 / vec2(textureSize(uDepth, 0));
    vec2 uv = vUV;

    // Sample center depth
    float centerDepth = texture(uDepth, uv).r;
    
    // Skip far plane/sky
    if (centerDepth >= 0.9999) {
        outOcclusion = 1.0;
        return;
    }

    // Get view space position
    vec3 origin = getViewPosition(uv, centerDepth);
    
    // Compute view-space normal from depth derivatives
    vec3 p1 = getViewPosition(uv + vec2(texelSize.x, 0.0), texture(uDepth, uv + vec2(texelSize.x, 0.0)).r);
    vec3 p2 = getViewPosition(uv + vec2(0.0, texelSize.y), texture(uDepth, uv + vec2(0.0, texelSize.y)).r);
    vec3 normal = normalize(cross(p2 - origin, p1 - origin));

    // Random rotation angle per pixel
    float randomAngle = hash(gl_FragCoord.xy) * 6.28318530718;
    vec2 randomVec = vec2(cos(randomAngle), sin(randomAngle));

    float occlusion = 0.0;
    float sampleRadius = pc.radius * texelSize.x * 100.0; // Convert to UV space

    for (int i = 0; i < 16; ++i) {
        // Rotate sample
        vec2 sampleDir = poisson[i];
        vec2 rotated = vec2(
            sampleDir.x * randomVec.x - sampleDir.y * randomVec.y,
            sampleDir.x * randomVec.y + sampleDir.y * randomVec.x
        );
        
        vec2 sampleUV = uv + rotated * sampleRadius;
        
        // Skip out of bounds
        if (sampleUV.x < 0.0 || sampleUV.x > 1.0 || sampleUV.y < 0.0 || sampleUV.y > 1.0) {
            continue;
        }

        // Sample depth
        float sampleDepth = texture(uDepth, sampleUV).r;
        vec3 samplePos = getViewPosition(sampleUV, sampleDepth);

        // Vector from origin to sample
        vec3 diff = samplePos - origin;
        float distance = length(diff);
        vec3 diffNorm = diff / distance;

        // Angle-based occlusion with falloff
        float normalCheck = max(dot(normal, diffNorm), 0.0);
        
        // Range check - fade out at radius distance
        float rangeCheck = smoothstep(0.0, 1.0, pc.radius / distance);
        
        // Check if sample is closer (occluding)
        // In view space, Z is negative and more negative = further
        float depthDiff = origin.z - samplePos.z;
        float occlusionFactor = step(pc.bias, depthDiff);
        
        occlusion += occlusionFactor * normalCheck * rangeCheck;
    }

    // Average and apply contrast
    occlusion = 1.0 - (occlusion / 16.0);
    occlusion = pow(occlusion, pc.intensity);
    
    outOcclusion = clamp(occlusion, 0.0, 1.0);
}
