#version 330 core
layout(location = 0) in vec3 a_pos;
layout(location = 1) in vec4 a_col;
uniform mat4 u_viewProj;
out vec4 v_col;
out vec3 v_pos;
void main() {
    v_col = a_col;
    v_pos = a_pos;
    gl_Position = u_viewProj * vec4(a_pos, 1.0);
}
