#version 450

layout(location = 0) in vec2 fragUV;
layout(location = 0) out vec4 outColor;

layout(binding = 0) uniform sampler2D glyphAtlas;

layout(push_constant) uniform PushConstants {
    vec2 screenSize;
    vec2 textColor;  // RG in first vec2
    vec2 textColor2; // BA in second vec2
} push;

void main() {
    float alpha = texture(glyphAtlas, fragUV).r;
    vec4 color = vec4(push.textColor.x, push.textColor.y, push.textColor2.x, push.textColor2.y);
    outColor = vec4(color.rgb, color.a * alpha);
}

