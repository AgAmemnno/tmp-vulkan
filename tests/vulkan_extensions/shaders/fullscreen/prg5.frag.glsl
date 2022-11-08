#version 460

layout(set = 0, binding = 0) uniform sampler2D samplerColorMap;

layout(location = 0) in vec3 inNormal;
layout(location = 0) out vec4 outFragColor;

void main()
{
    vec3 N = normalize(inNormal);
    outFragColor = vec4(N, 1.0);
}

