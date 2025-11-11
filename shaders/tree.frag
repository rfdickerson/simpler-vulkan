#version 450

// Input from vertex shader
layout(location = 0) in vec3 fragWorldPos;
layout(location = 1) in vec3 fragNormal;

// Output color
layout(location = 0) out vec4 outColor;

void main() {
    // Simple green color for trees
    vec3 baseColor = vec3(0.2, 0.6, 0.2);
    
    // Hardcoded sun direction for basic lighting
    vec3 sunDirection = normalize(vec3(0.3, -0.5, 0.4));
    vec3 sunColor = vec3(1.0, 0.95, 0.8);
    
    // Simple diffuse lighting
    vec3 N = normalize(fragNormal);
    vec3 L = normalize(-sunDirection);
    float NdotL = max(dot(N, L), 0.0);
    
    vec3 diffuse = baseColor * sunColor * NdotL;
    vec3 ambient = baseColor * 0.3;
    
    vec3 finalColor = ambient + diffuse;
    
    outColor = vec4(finalColor, 1.0);
}

