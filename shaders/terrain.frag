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
            baseColor = vec3(0.25, 0.75, 0.85); // Mediterranean turquoise
            break;
        case 1: // CoastalWater
            baseColor = vec3(0.3, 0.8, 0.9); // Lighter Mediterranean turquoise
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
            baseColor = vec3(0.28, 0.75, 0.88); // Turquoise river
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

// Simple procedural sky model to serve as environment for reflections
vec3 sampleSky(vec3 dir) {
	vec3 d = normalize(dir);
	float t = clamp(d.y * 0.5 + 0.5, 0.0, 1.0); // up factor

	// Horizon and zenith colors
	vec3 horizon = vec3(0.72, 0.86, 0.97);
	vec3 zenith  = vec3(0.18, 0.32, 0.58);
	vec3 skyBase = mix(horizon, zenith, t);

	// Soft sun glow in the direction of the sun
	vec3 sunDir = normalize(-terrain.sunDirection);
	float sunAmt = max(dot(sunDir, d), 0.0);
	float sunDisc = pow(sunAmt, 900.0) * 6.0;      // toned down core
	float sunHalo = pow(sunAmt, 8.0) * 0.25;       // softer halo
	vec3 sunColor = terrain.sunColor;

	return skyBase + sunColor * (sunHalo + sunDisc);
}

// Schlick Fresnel approximation
float fresnelSchlick(float cosTheta, float F0) {
	return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}

void main() {
    // Get base terrain color based on terrain type
    vec3 baseColor = getTerrainColor(fragTerrainType, fragHexCoord);
    
    // Calculate lighting
    vec3 litColor = calculateLighting(baseColor, fragNormal, fragWorldPos);

	// Reflective water from procedural environment sky
	bool isWater = (fragTerrainType == 0u || fragTerrainType == 1u || fragTerrainType == 14u);
	if (isWater) {
		// Base normal and enhanced animated perturbation for ripples
		vec3 N = normalize(fragNormal);
		float t = pc.time;
		
		// Multi-octave wave pattern for more realistic ripples
		vec2 waveCoord = fragWorldPos.xz * 0.15;
		float wave1 = noise(waveCoord + vec2(t * 0.08, 0.0));
		float wave2 = noise(waveCoord * 1.5 + vec2(0.0, t * 0.12));
		float wave3 = noise(waveCoord * 2.3 + vec2(t * 0.05, t * 0.09));
		
		// Combine waves with different scales and directions
		vec2 waveDir1 = vec2(cos(t * 0.1), sin(t * 0.1));
		vec2 waveDir2 = vec2(cos(t * 0.15 + 1.5), sin(t * 0.15 + 1.5));
		
		// Create directional wave patterns
		float waveHeight1 = (wave1 - 0.5) * 2.0;
		float waveHeight2 = (wave2 - 0.5) * 2.0;
		float waveHeight3 = (wave3 - 0.5) * 2.0;
		
		// Combine into normal perturbation
		vec3 ripple = vec3(
			waveHeight1 * waveDir1.x + waveHeight2 * waveDir2.x + waveHeight3 * 0.3,
			0.0,
			waveHeight1 * waveDir1.y + waveHeight2 * waveDir2.y + waveHeight3 * 0.3
		);
		
		// Slightly toned ripple strength for stability
		vec3 Np = normalize(N + ripple * 0.30);

		// View and reflection
		vec3 V = normalize(pc.cameraPos - fragWorldPos);
		vec3 R = reflect(-V, Np);

		// Sample procedural sky environment
		vec3 env = sampleSky(R);

		// Fresnel for dielectric water (low F0) with roughness moderation
		float NdotV = max(dot(Np, V), 0.0);
		float F0 = 0.02;
		float F = fresnelSchlick(NdotV, F0);
		float roughness = (fragTerrainType == 0u) ? 0.25 : ((fragTerrainType == 1u) ? 0.18 : 0.20);
		float reflectionStrength = F * (1.0 - 0.5 * roughness);

		// Horizon fade so reflections don't look like chrome near the horizon
		float horizonFade = smoothstep(0.0, 0.25, R.y);
		reflectionStrength *= horizonFade;

		// Turquoise transmission color with absorption (red is absorbed most in water)
		vec3 waterColor = baseColor;
		vec3 absorption = vec3(2.2, 0.35, 0.05);
		float viewDepth = pow(1.0 - NdotV, 1.2) * 2.0; // cheap thickness approximation
		vec3 transmittance = exp(-absorption * viewDepth);
		float shallowBoost = 0.35 + 0.35 * clamp(Np.y, 0.0, 1.0);
		vec3 transmission = waterColor * transmittance * shallowBoost * terrain.ambientIntensity;
		
		// Specular highlight from direct sun (non-metallic, roughness-controlled)
		vec3 L = normalize(-terrain.sunDirection);
		float shininess = mix(24.0, 96.0, 1.0 - roughness);
		float specular = pow(max(dot(reflect(-L, Np), V), 0.0), shininess);
		vec3 sunSpec = terrain.sunColor * specular * 0.15;

		// Slightly tint and soften environment reflection
		env = mix(env, waterColor, 0.15 * roughness) * 0.9;

		// Blend reflection with transmission using moderated Fresnel
		litColor = mix(transmission + sunSpec, env, clamp(reflectionStrength, 0.0, 0.9));
	}
    
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

