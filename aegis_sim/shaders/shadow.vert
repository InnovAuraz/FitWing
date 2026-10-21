#version 330 core
layout(location = 0) in vec3 a_pos;
layout(location = 2) in vec2 a_uv;
#ifdef INSTANCED
layout(location = 4) in mat4 a_inst;
#endif
uniform mat4 u_viewProj;
uniform mat4 u_model;
out vec2 v_uv;
void main() {
#ifdef INSTANCED
    mat4 M = a_inst * u_model;
#else
    mat4 M = u_model;
#endif
    v_uv = a_uv;
    gl_Position = u_viewProj * M * vec4(a_pos, 1.0);
}
