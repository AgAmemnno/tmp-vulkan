#version 460

layout(push_constant, std430) uniform shaderInformation
{
    int tonemapper;
    float gamma;
    float exposure;
} pushc;

layout(set = 0, binding = 0) uniform sampler2D noisyTxt;

layout(location = 0) in vec2 outUV;
layout(location = 0) out vec4 fragColor;

void main()
{
    vec2 uv = outUV;
    vec4 color = texture(noisyTxt, uv);
    fragColor = vec4(uv, 0.0, 1.0);
}

