#version 450

// Input from vertex shader
layout(location = 0) in vec3 fragWorldPos;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec2 fragUV;
layout(location = 3) in vec2 fragHexCoord;
layout(location = 4) flat in uint fragTerrainType;

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

// Axial (q, r) -> world XZ center for flat-top hex layout
vec2 axialToWorldXZFlatTop(vec2 qr, float size) {
	// Matches src/hex_coord.hpp::hexToWorld (note the negated Z)
	float q = qr.x;
	float r = qr.y;
	float x = size * (1.5 * q);
	float zRaw = size * (sqrt(3.0) * 0.5 * q + sqrt(3.0) * r);
	return vec2(x, -zRaw);
}

// Signed distance to a flat-top regular hex with circumradius r, centered at origin
// Adapted from well-known SDF formulations (Inigo Quilez)
float sdHexFlatTop(vec2 p, float r) {
	// Project into first sextant
	p = abs(p);
	// Distance to vertical and slanted edges
	float d1 = p.y;
	float d2 = p.x * 0.57735026919 + p.y * 0.5; // tan(30) = 1/sqrt(3), cos(60)=0.5
	float h = max(d1, d2) - r * 0.5; // scale to match circumradius
	// The coefficient 0.5 aligns the edge at radius r in this formulation
	return h;
}

// Get terrain color based on terrain type
vec3 getTerrainColor(uint terrainType, vec2 hexCoord) {
    // Terrain type colors (matching TerrainProperties in terrain.hpp)
    vec3 baseColor;
    
    switch(terrainType) {
        case 0: // Ocean
            baseColor = vec3(0.1, 0.3, 0.5);
            break;
        case 1: // CoastalWater
            baseColor = vec3(0.2, 0.5, 0.6);
            break;
        case 2: // Grassland
            baseColor = vec3(0.4, 0.6, 0.2);
            break;
        case 3: // Plains
            baseColor = vec3(0.7, 0.6, 0.3);
            break;
        case 4: // Forest
            baseColor = vec3(0.2, 0.4, 0.1);
            break;
        case 5: // Jungle
            baseColor = vec3(0.15, 0.35, 0.15);
            break;
        case 6: // Hills
            baseColor = vec3(0.5, 0.5, 0.3);
            break;
        case 7: // Mountains
            baseColor = vec3(0.4, 0.4, 0.4);
            break;
        case 8: // Desert
            baseColor = vec3(0.9, 0.8, 0.5);
            break;
        case 9: // Dunes
            baseColor = vec3(0.95, 0.85, 0.6);
            break;
        case 10: // Swamp
            baseColor = vec3(0.3, 0.3, 0.2);
            break;
        case 11: // Marsh
            baseColor = vec3(0.4, 0.4, 0.3);
            break;
        case 12: // Tundra
            baseColor = vec3(0.8, 0.85, 0.9);
            break;
        case 13: // Ice
            baseColor = vec3(0.9, 0.95, 1.0);
            break;
        case 14: // River
            baseColor = vec3(0.3, 0.5, 0.7);
            break;
        case 15: // NaturalWonder
            baseColor = vec3(0.8, 0.6, 1.0);
            break;
        default:
            baseColor = vec3(1.0, 0.0, 1.0); // Magenta for unknown types
            break;
    }
    
    // Add subtle procedural variation
    float detail = noise(hexCoord * 2.0);
    baseColor = mix(baseColor, baseColor * 1.2, detail * 0.15);
    
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

void main() {
    // Get base terrain color based on terrain type
    vec3 baseColor = getTerrainColor(fragTerrainType, fragHexCoord);
    
    // Calculate lighting
    vec3 litColor = calculateLighting(baseColor, fragNormal, fragWorldPos);
    
	// Compute precise hex-edge factor using SDF in local hex coordinates
	vec2 centerXZ = axialToWorldXZFlatTop(fragHexCoord, terrain.hexSize);
	vec2 local = fragWorldPos.xz - centerXZ;
	// The mesh uses circumradius = hexSize; SDF expects same scale
	float d = sdHexFlatTop(local, terrain.hexSize);

	// Outline near the hex edge (anti-aliased)
	float edgePx = 1.5; // edge thickness in "world" units scaled by derivative
	// Convert to a smoothing width using screen-space derivatives
	float dd = abs(d);
	float aa = fwidth(d) * 1.5;
	float edgeMask = 1.0 - smoothstep(edgePx - aa, edgePx + aa, dd);

	// Subtle inner darkening near edges for inked-map look
	float innerShade = 1.0 - smoothstep(0.0, edgePx * 2.5 + aa, dd);
	vec3 shaded = mix(litColor * 0.92, litColor, innerShade);

	// Warm edge tint mixed in only at the very rim
	vec3 edgeColor = vec3(0.85, 0.78, 0.65);
	shaded = mix(shaded, edgeColor, edgeMask * 0.18);
    
    // Era-based color grading (simplified)
    if (terrain.currentEra == 1) { // Enlightenment
		shaded = mix(shaded, vec3(dot(shaded, vec3(0.299, 0.587, 0.114))), 0.2);
    } else if (terrain.currentEra == 2) { // Industrial
		shaded *= vec3(0.8, 0.85, 0.9); // Cool, desaturated
		shaded = mix(shaded, vec3(dot(shaded, vec3(0.299, 0.587, 0.114))), 0.4);
    }
    
    // Output final color
	outColor = vec4(shaded, 1.0);
}

