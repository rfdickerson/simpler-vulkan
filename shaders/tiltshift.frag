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

// Simple hash for per-pixel angle jitter
float hash12(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453123); }

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

    // Golden-angle jittered disk sampling with per-pixel rotation to reduce banding
    float ang0 = hash12(uv) * 6.28318530718;

    const int SAMPLES = 24;
    const float GA = 2.39996322973; // golden angle

    vec2 texel = 1.0 / pc.resolution;
    vec3 sum = vec3(0.0);
    float wsum = 0.0;

    float sigma = max(radiusPx * 0.5, 1.0);
    float invTwoSigma2 = 0.5 / (sigma * sigma);

    for (int i = 0; i < SAMPLES; ++i) {
        float t = (float(i) + 0.5) / float(SAMPLES); // 0..1
        float r = t;                                  // radial progression
        float a = ang0 + GA * float(i);
        vec2 dir = vec2(cos(a), sin(a));

        float distPx = r * radiusPx;
        float w = exp(-(distPx * distPx) * invTwoSigma2); // Gaussian weight in pixel space
        vec2 duv = dir * (distPx) * texel;

        vec3 c = texture(uSceneColor, uv + duv).rgb;
        sum += c * w;
        wsum += w;
    }

    vec3 blurred = sum / max(wsum, 1e-5);

    // Smooth blend amount based on blur radius
    float t = smoothstep(0.0, pc.maxRadiusPixels, radiusPx);

    // Apply the blur blend
    vec3 finalColor = mix(base, blurred, t);
    outColor = vec4(finalColor, 1.0);
}


