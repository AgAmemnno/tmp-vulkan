#version 460

const vec3 _20[4] = vec3[](vec3(-1.0, -1.0, 0.0), vec3(1.0, -1.0, 0.0), vec3(-1.0, 1.0, 0.0), vec3(1.0, 1.0, 0.0));

layout(location = 0) out vec2 oUV;

void main()
{
    gl_Position = vec4(_20[gl_VertexIndex], 1.0);
    gl_Position.y *= (-1.0);
    oUV = (gl_Position.xy * 0.5) + vec2(0.5);
}

