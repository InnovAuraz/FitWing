#version 330 core
layout(location = 0) in vec3 a_pos;
layout(location = 2) in vec2 a_uv;   // x = water depth in metres (negative on land)
uniform mat4 u_viewProj;
out vec3 v_pos;
out float v_depth;
void main() {
    v_pos = a_pos;
    v_depth = a_uv.x;
    gl_Position = u_viewProj * vec4(a_pos, 1.0);
}
