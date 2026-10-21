#version 330 core
layout(location = 0) in vec3 a_pos;
layout(location = 1) in vec3 a_nrm;
uniform mat4 u_viewProj;
out vec3 v_pos;
out vec3 v_nrm;
void main() {
    v_pos = a_pos;
    v_nrm = a_nrm;
    gl_Position = u_viewProj * vec4(a_pos, 1.0);
}
