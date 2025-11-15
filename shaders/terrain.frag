#version 450

layout(location = 0) in vec3 fragWorldPos;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec2 fragUV;
layout(location = 3) in vec2 fragHexCoord;
layout(location = 4) flat in uint fragTerrainType;

layout(push_constant) uniform PushConstants {
    mat4 viewProj;
    vec3 cameraPos;
    float time;
} pc;

layout(binding = 0) uniform TerrainParams {
    vec4 sunDirSnow;        // xyz = sun direction, w = snow line
    vec4 sunColorAmbient;   // xyz = sun color, w = ambient intensity
    vec4 fieldScales;       // x = hex size, y = moisture noise, z = temperature noise, w = biome noise
    vec4 fieldBias;         // x = temperature lapse, y = moisture bias, z = slope rock strength, w = river rock bias
    vec4 elevationInfo;     // x = min elevation, y = max elevation, z = detail noise scale, w unused
    ivec4 eraInfo;          // x = current era index
} terrain;

layout(binding = 1) uniform sampler2D ssaoTex;
layout(location = 0) out vec4 outColor;

float hash(vec2 p) {
    vec3 p3 = fract(vec3(p.xyx) * 0.13);
    p3 += dot(p3, p3.yzx + 3.333);
    return fract((p3.x + p3.y) * p3.z);
}

float noise(vec2 p) {
    vec2 i = floor(p);
    vec2 f = fract(p);

    float a = hash(i);
    float b = hash(i + vec2(1.0, 0.0));
    float c = hash(i + vec2(0.0, 1.0));
    float d = hash(i + vec2(1.0, 1.0));

    vec2 u = f * f * (3.0 - 2.0 * f);

    return mix(a, b, u.x) + (c - a) * u.y * (1.0 - u.x) + (d - b) * u.x * u.y;
}

float fbm(vec2 p) {
    float value = 0.0;
    float amplitude = 0.5;
    float frequency = 1.0;
    for (int i = 0; i < 4; ++i) {
        value += amplitude * noise(p * frequency);
        frequency *= 2.03;
        amplitude *= 0.5;
    }
    return value;
}

float saturate(float x) {
    return clamp(x, 0.0, 1.0);
}

vec2 axialToWorldXZFlatTop(vec2 qr, float size) {
    float q = qr.x;
    float r = qr.y;
    float x = size * (1.5 * q);
    float zRaw = size * (sqrt(3.0) * 0.5 * q + sqrt(3.0) * r);
    return vec2(x, -zRaw);
}

float sdHexFlatTop(vec2 p, float r) {
    p = abs(p);
    float d1 = p.y;
    float d2 = p.x * 0.57735026919 + p.y * 0.5;
    float h = max(d1, d2) - r * 0.5;
    return h;
}

float terrainMoistureBias(uint terrainType) {
    switch (terrainType) {
        case 0u:
        case 1u:
        case 14u:
            return 0.45;
        case 4u:
        case 5u:
            return 0.2;
        case 10u:
        case 11u:
            return 0.3;
        case 12u:
        case 13u:
            return 0.15;
        case 8u:
        case 9u:
            return -0.55;
        default:
            return 0.0;
    }
}

float terrainTemperatureBias(uint terrainType) {
    switch (terrainType) {
        case 8u:
        case 9u:
            return 0.3;
        case 5u:
            return 0.12;
        case 6u:
        case 7u:
            return -0.1;
        case 12u:
        case 13u:
            return -0.45;
        default:
            return 0.0;
    }
}

float terrainSandPreference(uint terrainType) {
    switch (terrainType) {
        case 8u:
        case 9u:
            return 0.7;
        case 1u:
            return 0.2;
        default:
            return 0.0;
    }
}

struct MaterialSample {
    vec3 albedo;
    float roughness;
    vec3 normal;
};

MaterialSample makeMaterial(vec3 tint, float baseRoughness, float normalStrength, vec2 uv, float tiling, float seed) {
    float tileScale = max(tiling, 0.0005);
    vec2 baseUv = uv * tileScale + vec2(seed * 0.618, seed * 1.382);
    float colorNoise = fbm(baseUv);
    float tonalNoise = noise(baseUv * 1.93 + vec2(12.4, 7.1));
    float ridgeNoise = noise(baseUv * 2.73 + vec2(5.7, 16.9));

    vec3 albedo = tint * (0.82 + 0.36 * colorNoise);
    albedo = mix(albedo, tint * 1.15, tonalNoise * 0.18);

    float rough = clamp(baseRoughness + (tonalNoise - 0.5) * 0.22, 0.04, 0.95);

    float nx = ridgeNoise - 0.5;
    float nz = noise(baseUv * 2.11 + vec2(21.3, 3.4)) - 0.5;
    vec3 detailNormal = normalize(vec3(nx * normalStrength, 1.0, nz * normalStrength));

    return MaterialSample(albedo, rough, detailNormal);
}

vec3 getWaterTint(uint terrainType, float moisture, float temperature) {
    vec3 ocean = vec3(0.14, 0.55, 0.74);
    vec3 coast = vec3(0.20, 0.74, 0.88);
    vec3 river = vec3(0.18, 0.68, 0.84);
    if (terrainType == 0u) {
        return mix(ocean, vec3(0.12, 0.34, 0.52), saturate(1.0 - temperature));
    } else if (terrainType == 1u) {
        return mix(coast, vec3(0.36, 0.84, 0.92), moisture * 0.3);
    } else if (terrainType == 14u) {
        return mix(river, coast, moisture * 0.25);
    }
    return coast;
}

vec3 sampleSky(vec3 dir) {
    vec3 d = normalize(dir);
    float t = saturate(d.y * 0.5 + 0.5);
    vec3 horizon = vec3(0.72, 0.86, 0.97);
    vec3 zenith = vec3(0.18, 0.32, 0.58);
    vec3 skyBase = mix(horizon, zenith, t);

    vec3 sunDir = normalize(-terrain.sunDirSnow.xyz);
    float sunAmt = max(dot(sunDir, d), 0.0);
    float sunDisc = pow(sunAmt, 900.0) * 6.0;
    float sunHalo = pow(sunAmt, 8.0) * 0.25;
    vec3 sunColor = terrain.sunColorAmbient.xyz;

    return skyBase + sunColor * (sunHalo + sunDisc);
}

float fresnelSchlick(float cosTheta, float F0) {
    return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}

float computeMoisture(uint terrainType, float slope, float riverMask, float wetlandsMask, vec2 fieldUv) {
    float base = fbm(fieldUv * terrain.fieldScales.y + vec2(0.17, 0.31));
    float moisture = 0.5 + (base - 0.5) * 0.85;
    moisture += (1.0 - slope) * 0.25;
    moisture += terrain.fieldBias.y;
    moisture += terrainMoistureBias(terrainType);
    moisture += wetlandsMask * 0.25;
    moisture = saturate(moisture);
    moisture = mix(moisture, 1.0, riverMask * 0.6);
    return moisture;
}

float computeTemperature(uint terrainType, float elevationNorm, vec2 fieldUv) {
    vec2 tempUv = fieldUv * terrain.fieldScales.z + vec2(1.3, 2.7);
    float band = 0.5 - fragWorldPos.z * (terrain.fieldScales.z * 0.85);
    float noiseTerm = fbm(tempUv);
    float temperature = band + (noiseTerm - 0.5) * 0.6;
    temperature += terrainTemperatureBias(terrainType);
    temperature -= elevationNorm * terrain.fieldBias.x;
    return saturate(temperature);
}

void applyEraGrading(inout vec3 color) {
    int era = terrain.eraInfo.x;
    if (era == 1) {
        float luma = dot(color, vec3(0.299, 0.587, 0.114));
        color = mix(color, vec3(luma), 0.2);
    } else if (era == 2) {
        float luma = dot(color, vec3(0.299, 0.587, 0.114));
        color *= vec3(0.8, 0.85, 0.9);
        color = mix(color, vec3(luma), 0.4);
    }
}

void main() {
    vec3 Nbase = normalize(fragNormal);
    vec3 up = vec3(0.0, 1.0, 0.0);
    float slope = saturate(1.0 - dot(Nbase, up));

    float minElevation = terrain.elevationInfo.x;
    float maxElevation = terrain.elevationInfo.y;
    float elevationRange = max(maxElevation - minElevation, 0.001);
    float elevationNorm = saturate((fragWorldPos.y - minElevation) / elevationRange);

    vec2 biomeUv = fragWorldPos.xz * terrain.fieldScales.w + fragHexCoord * 0.11;

    float riverMask = (fragTerrainType == 14u) ? 1.0 : 0.0;
    float coastMask = (fragTerrainType == 1u) ? 1.0 : 0.0;
    float wetlandsMask = (fragTerrainType == 10u || fragTerrainType == 11u) ? 1.0 : 0.0;
    float desertMask = (fragTerrainType == 8u || fragTerrainType == 9u) ? 1.0 : 0.0;
    float hillMask = (fragTerrainType == 6u || fragTerrainType == 7u) ? 1.0 : 0.0;
    float coldMask = (fragTerrainType == 12u || fragTerrainType == 13u) ? 1.0 : 0.0;
    float forestMask = (fragTerrainType == 4u || fragTerrainType == 5u) ? 1.0 : 0.0;

    bool isWater = (fragTerrainType == 0u || fragTerrainType == 1u || fragTerrainType == 14u);

    float moisture = computeMoisture(fragTerrainType, slope, riverMask, wetlandsMask, biomeUv);
    float temperature = computeTemperature(fragTerrainType, elevationNorm, biomeUv);

    if (isWater) {
        vec3 N = normalize(fragNormal);
        float t = pc.time;
        vec2 waveCoord = fragWorldPos.xz * 0.15;
        float wave1 = noise(waveCoord + vec2(t * 0.08, 0.0));
        float wave2 = noise(waveCoord * 1.5 + vec2(0.0, t * 0.12));
        float wave3 = noise(waveCoord * 2.3 + vec2(t * 0.05, t * 0.09));

        vec2 waveDir1 = vec2(cos(t * 0.1), sin(t * 0.1));
        vec2 waveDir2 = vec2(cos(t * 0.15 + 1.5), sin(t * 0.15 + 1.5));

        float waveHeight1 = (wave1 - 0.5) * 2.0;
        float waveHeight2 = (wave2 - 0.5) * 2.0;
        float waveHeight3 = (wave3 - 0.5) * 2.0;

        vec3 ripple = vec3(
            waveHeight1 * waveDir1.x + waveHeight2 * waveDir2.x + waveHeight3 * 0.3,
            0.0,
            waveHeight1 * waveDir1.y + waveHeight2 * waveDir2.y + waveHeight3 * 0.3);

        vec3 Np = normalize(N + ripple * 0.30);

        vec3 V = normalize(pc.cameraPos - fragWorldPos);
        vec3 R = reflect(-V, Np);
        vec3 env = sampleSky(R);

        float NdotV = max(dot(Np, V), 0.0);
        float F0 = 0.02;
        float F = fresnelSchlick(NdotV, F0);
        float roughness = (fragTerrainType == 0u) ? 0.25 : ((fragTerrainType == 1u) ? 0.18 : 0.20);
        float reflectionStrength = F * (1.0 - 0.5 * roughness);

        float horizonFade = smoothstep(0.0, 0.25, R.y);
        reflectionStrength *= horizonFade;

        vec3 waterColor = getWaterTint(fragTerrainType, moisture, temperature);
        vec3 absorption = vec3(1.8, 0.25, 0.04);
        float viewDepth = pow(1.0 - NdotV, 1.2) * 2.0;
        vec3 transmittance = exp(-absorption * viewDepth);
        float shallowBoost = 0.50 + 0.50 * saturate(Np.y);
        vec3 transmission = waterColor * transmittance * shallowBoost * terrain.sunColorAmbient.w;

        vec3 L = normalize(-terrain.sunDirSnow.xyz);
        float shininess = mix(24.0, 96.0, 1.0 - roughness);
        float specular = pow(max(dot(reflect(-L, Np), V), 0.0), shininess);
        vec3 sunSpec = terrain.sunColorAmbient.xyz * specular * 0.20;

        env = mix(env, waterColor, 0.25 * roughness);

        vec2 ssaoSize = vec2(textureSize(ssaoTex, 0));
        float ao = texture(ssaoTex, gl_FragCoord.xy / ssaoSize).r;
        ao = pow(ao, 2.0);

        vec3 litColor = mix(transmission + sunSpec, env, clamp(reflectionStrength, 0.0, 0.8));
        litColor *= mix(vec3(1.0), vec3(ao), 0.65);

        vec2 centerXZ = axialToWorldXZFlatTop(fragHexCoord, terrain.fieldScales.x);
        vec2 local = fragWorldPos.xz - centerXZ;
        float d = sdHexFlatTop(local, terrain.fieldScales.x);
        float edgePx = 1.5;
        float dd = abs(d);
        float aa = fwidth(d) * 1.5;
        float edgeMask = 1.0 - smoothstep(edgePx - aa, edgePx + aa, dd);
        float innerShade = 1.0 - smoothstep(0.0, edgePx * 2.5 + aa, dd);
        vec3 edgeColor = vec3(0.85, 0.78, 0.65);
        litColor = mix(litColor * 0.94, litColor, innerShade);
        litColor = mix(litColor, edgeColor, edgeMask * 0.12);

        applyEraGrading(litColor);
        outColor = vec4(litColor, 1.0);
        return;
    }

    moisture = mix(moisture, moisture * 0.8, desertMask);
    moisture = mix(moisture, saturate(moisture + 0.15), forestMask);

    float snowLine = terrain.sunDirSnow.w;
    float wSnow = smoothstep(snowLine - 0.1, snowLine + 0.05, elevationNorm + coldMask * 0.05);

    float wGrass = smoothstep(0.25, 0.65, moisture) * (1.0 - smoothstep(0.15, 0.55, slope));
    wGrass += forestMask * 0.15;

    float slopeBias = mix(1.0, 1.0 + slope * 1.8, terrain.fieldBias.z);
    slopeBias = mix(slopeBias, slopeBias * 1.25, hillMask);
    float wRock = smoothstep(0.35, 0.75, slope) * slopeBias;

    float sandPref = terrainSandPreference(fragTerrainType);
    float wSand = smoothstep(0.65, 0.25, moisture) * smoothstep(0.18, 0.0, slope);
    wSand *= smoothstep(0.4, 0.85, temperature + sandPref * 0.2);
    wSand += sandPref;
    wSand += coastMask * 0.1;

    wRock += riverMask * terrain.fieldBias.w * 0.6;
    wSand += riverMask * terrain.fieldBias.w * 0.4;

    wGrass *= (1.0 - wSnow);
    wRock *= (1.0 - wSnow);
    wSand *= (1.0 - wSnow);

    float total = wGrass + wRock + wSand + wSnow;
    if (total < 1e-4) {
        wGrass = 1.0;
        total = 1.0;
    }
    wGrass /= total;
    wRock /= total;
    wSand /= total;
    wSnow /= total;

    float detailScale = terrain.elevationInfo.z;
    MaterialSample grass = makeMaterial(vec3(0.38, 0.58, 0.24), 0.55, 0.6 + moisture * 0.3, biomeUv, detailScale * 0.8, 1.0);
    MaterialSample rock = makeMaterial(vec3(0.45, 0.43, 0.41), 0.68, 1.0 + slope * 1.2, biomeUv, detailScale * 0.55, 7.0);
    MaterialSample sand = makeMaterial(vec3(0.87, 0.78, 0.52), 0.82, 0.55, biomeUv, detailScale * 1.1, 11.0);
    MaterialSample snow = makeMaterial(vec3(0.92, 0.95, 1.02), 0.18, 0.35, biomeUv, detailScale * 0.45, 19.0);

    vec3 blendedAlbedo =
        wGrass * grass.albedo +
        wRock * rock.albedo +
        wSand * sand.albedo +
        wSnow * snow.albedo;

    float blendedRoughness =
        wGrass * grass.roughness +
        wRock * rock.roughness +
        wSand * sand.roughness +
        wSnow * snow.roughness;

    vec3 detailNormal = normalize(
        wGrass * grass.normal +
        wRock * rock.normal +
        wSand * sand.normal +
        wSnow * snow.normal);

    float detailWeight = mix(0.25, 0.55, slope);
    vec3 N = normalize(mix(Nbase, detailNormal, detailWeight));

    blendedAlbedo = mix(blendedAlbedo, blendedAlbedo * vec3(1.05, 1.02, 0.98), forestMask * 0.2);
    blendedAlbedo = mix(blendedAlbedo, vec3(0.94, 0.92, 0.88), wSnow * 0.2);
    blendedAlbedo *= mix(0.92, 1.08, moisture);

    vec3 L = normalize(-terrain.sunDirSnow.xyz);
    vec3 V = normalize(pc.cameraPos - fragWorldPos);
    vec3 H = normalize(L + V);

    float NdotL = max(dot(N, L), 0.0);
    float NdotH = max(dot(N, H), 0.0);

    vec3 diffuse = blendedAlbedo * terrain.sunColorAmbient.xyz * NdotL;
    vec3 ambient = blendedAlbedo * terrain.sunColorAmbient.w;

    float specPower = mix(120.0, 12.0, blendedRoughness);
    float specStrength = mix(0.04, 0.16, 1.0 - blendedRoughness);
    vec3 specular = terrain.sunColorAmbient.xyz * specStrength * pow(NdotH, specPower);

    vec3 litColor = ambient + diffuse + specular;

    vec2 ssaoSize = vec2(textureSize(ssaoTex, 0));
    float ao = texture(ssaoTex, gl_FragCoord.xy / ssaoSize).r;
    ao = pow(ao, 2.0);
    litColor *= mix(vec3(1.0), vec3(ao), 0.85);

    vec3 fresnelTint = vec3(0.02);
    float horizon = saturate(1.0 - dot(N, up));
    litColor += fresnelTint * pow(horizon, 4.0) * 0.25;

    vec2 centerXZ = axialToWorldXZFlatTop(fragHexCoord, terrain.fieldScales.x);
    vec2 local = fragWorldPos.xz - centerXZ;
    float d = sdHexFlatTop(local, terrain.fieldScales.x);
    float edgePx = 1.5;
    float dd = abs(d);
    float aa = fwidth(d) * 1.5;
    float edgeMask = 1.0 - smoothstep(edgePx - aa, edgePx + aa, dd);
    float innerShade = 1.0 - smoothstep(0.0, edgePx * 2.5 + aa, dd);
    vec3 edgeColor = vec3(0.85, 0.78, 0.65);
    litColor = mix(litColor * 0.92, litColor, innerShade);
    litColor = mix(litColor, edgeColor, edgeMask * 0.18);

    applyEraGrading(litColor);
    outColor = vec4(litColor, 1.0);
}
