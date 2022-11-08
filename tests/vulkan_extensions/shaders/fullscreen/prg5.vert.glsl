#version 460

layout(location = 0) in vec3 inPos;
layout(location = 0) out vec3 outNormal;
layout(location = 1) in vec3 inNormal;

void main()
{
    vec4 pos = vec4(inPos, 1.0);
    gl_Position = pos;
    gl_Position.y = -gl_Position.y;
    outNormal = inNormal;
}

