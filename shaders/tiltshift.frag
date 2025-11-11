#version 450

layout(location = 0) in vec2 vUV;
layout(location = 0) out vec4 outColor;

layout(binding = 0) uniform sampler2D uSceneColor;
layout(binding = 1) uniform sampler2D uDepth;

layout(push_constant) uniform TiltShiftPC {
    float cosAngle;
    float sinAngle;
    float focusCenter;
    float bandHalfWidth;
    float blurScale;
    float maxRadiusPixels;
    vec2 resolution;
    float padding;
} pc;

void main() {
    vec2 uv = vUV;
    
    // Rotate around center (0.5,0.5) by tilt angle
    vec2 centered = uv - 0.5;
    vec2 rotated;
    rotated.x =  centered.x * pc.cosAngle + centered.y * pc.sinAngle;
    rotated.y = -centered.x * pc.sinAngle + centered.y * pc.cosAngle;
    rotated += 0.5;

    // Distance from the focus line at y = focusCenter in rotated space
    float dist = abs(rotated.y - pc.focusCenter);

    // Compute blur radius in pixels outside the sharp band
    float excess = max(0.0, dist - pc.bandHalfWidth);
    // Convert normalized excess to pixel radius (scaled by vertical resolution for consistency)
    float radiusPx = clamp(excess * pc.blurScale * pc.resolution.y,
                           0.0, pc.maxRadiusPixels);

    // Debug: Visualize blur strength (white = max blur, black = no blur)
    // outColor = vec4(vec3(radiusPx / pc.maxRadiusPixels), 1.0); return;

    vec3 base = texture(uSceneColor, uv).rgb;
    if (radiusPx < 0.5) {
        outColor = vec4(base, 1.0);
        return;
    }

    // Poisson-like disk samples (normalized, roughly within unit circle)
    const vec2 OFFS[12] = vec2[](
        vec2( 0.0,  0.0),
        vec2( 0.5,  0.0),
        vec2(-0.5,  0.0),
        vec2( 0.0,  0.5),
        vec2( 0.0, -0.5),
        vec2( 0.35,  0.35),
        vec2(-0.35,  0.35),
        vec2( 0.35, -0.35),
        vec2(-0.35, -0.35),
        vec2( 1.0,  0.0),
        vec2( 0.0,  1.0),
        vec2(-1.0,  0.0)
    );

    vec2 texel = 1.0 / pc.resolution;
    vec3 sum = vec3(0.0);
    float wsum = 0.0;

    for (int i = 0; i < 12; ++i) {
        vec2 duv = OFFS[i] * (radiusPx * texel);
        vec3 c = texture(uSceneColor, uv + duv).rgb;
        float r2 = dot(OFFS[i], OFFS[i]);
        float w = exp(-r2); // simple radially decaying weight
        sum += c * w;
        wsum += w;
    }

    vec3 blurred = sum / max(wsum, 1e-5);

    // Blend amount based on how strong the blur radius is
    float t = clamp(radiusPx / pc.maxRadiusPixels, 0.0, 1.0);
    
    // Apply the blur blend
    vec3 finalColor = mix(base, blurred, t);
    outColor = vec4(finalColor, 1.0);
}


