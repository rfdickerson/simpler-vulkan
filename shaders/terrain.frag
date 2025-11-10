#version 450

// Input from vertex shader
layout(location = 0) in vec3 fragWorldPos;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec2 fragUV;
layout(location = 3) in vec2 fragHexCoord;

// Push constants
layout(push_constant) uniform PushConstants {
    mat4 viewProj;
    vec3 cameraPos;
    float time;
} pc;

// Uniform buffer for terrain parameters
layout(binding = 0) uniform TerrainParams {
    vec3 sunDirection;
    vec3 sunColor;
    float ambientIntensity;
    float hexSize;
    int currentEra;
    float _padding[2];
} terrain;

// Output color
layout(location = 0) out vec4 outColor;

// Procedural noise function (simple hash-based)
float hash(vec2 p) {
    vec3 p3 = fract(vec3(p.xyx) * 0.13);
    p3 += dot(p3, p3.yzx + 3.333);
    return fract((p3.x + p3.y) * p3.z);
}

// Smooth noise
float noise(vec2 p) {
    vec2 i = floor(p);
    vec2 f = fract(p);
    
    float a = hash(i);
    float b = hash(i + vec2(1.0, 0.0));
    float c = hash(i + vec2(0.0, 1.0));
    float d = hash(i + vec2(1.0, 1.0));
    
    vec2 u = f * f * (3.0 - 2.0 * f);
    
    return mix(a, b, u.x) + (c - a)* u.y * (1.0 - u.x) + (d - b) * u.x * u.y;
}

// Procedural terrain color based on hex coordinates
vec3 getTerrainColor(vec2 hexCoord) {
    // Use hex coordinates to generate varied terrain
    float n = noise(hexCoord * 0.5);
    
    // Base colors for different terrain types (simplified for now)
    vec3 grassColor = vec3(0.4, 0.6, 0.2);
    vec3 dirtColor = vec3(0.5, 0.4, 0.3);
    vec3 stoneColor = vec3(0.6, 0.6, 0.6);
    
    // Mix colors based on noise
    vec3 baseColor = mix(grassColor, dirtColor, smoothstep(0.3, 0.7, n));
    
    // Add some variation
    float detail = noise(hexCoord * 2.0);
    baseColor = mix(baseColor, stoneColor, detail * 0.2);
    
    return baseColor;
}

// Simple PBR-inspired lighting
vec3 calculateLighting(vec3 baseColor, vec3 normal, vec3 worldPos) {
    // Normalize inputs
    vec3 N = normalize(normal);
    vec3 L = normalize(-terrain.sunDirection);
    vec3 V = normalize(pc.cameraPos - worldPos);
    vec3 H = normalize(L + V);
    
    // Diffuse (Lambert)
    float NdotL = max(dot(N, L), 0.0);
    vec3 diffuse = baseColor * terrain.sunColor * NdotL;
    
    // Ambient
    vec3 ambient = baseColor * terrain.ambientIntensity;
    
    // Specular (simplified)
    float specularStrength = 0.1;
    float spec = pow(max(dot(N, H), 0.0), 32.0);
    vec3 specular = specularStrength * spec * terrain.sunColor;
    
    // Ambient occlusion (fake it with curvature)
    float ao = 0.5 + 0.5 * dot(N, vec3(0.0, 1.0, 0.0));
    
    return (ambient + diffuse + specular) * ao;
}

// Hex edge detection for border highlights
float hexEdgeDistance(vec2 uv) {
    // Distance from center
    vec2 centered = uv - vec2(0.5);
    float dist = length(centered);
    
    // Edge detection (closer to 0.5 = edge)
    float edgeDist = abs(dist - 0.45);
    return smoothstep(0.0, 0.05, edgeDist);
}

void main() {
    // Get base terrain color
    vec3 baseColor = getTerrainColor(fragHexCoord);
    
    // Calculate lighting
    vec3 litColor = calculateLighting(baseColor, fragNormal, fragWorldPos);
    
    // Hex border highlight (subtle edge light)
    float edgeFactor = hexEdgeDistance(fragUV);
    vec3 edgeColor = vec3(0.8, 0.7, 0.5); // Warm edge light (parchment-like)
    litColor = mix(edgeColor * 0.3, litColor, edgeFactor);
    
    // Era-based color grading (simplified)
    if (terrain.currentEra == 1) { // Enlightenment
        litColor = mix(litColor, vec3(dot(litColor, vec3(0.299, 0.587, 0.114))), 0.2);
    } else if (terrain.currentEra == 2) { // Industrial
        litColor *= vec3(0.8, 0.85, 0.9); // Cool, desaturated
        litColor = mix(litColor, vec3(dot(litColor, vec3(0.299, 0.587, 0.114))), 0.4);
    }
    
    // Output final color
    outColor = vec4(litColor, 1.0);
}

