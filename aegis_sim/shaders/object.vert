#version 330 core
layout(location = 0) in vec3 a_pos;
layout(location = 1) in vec3 a_nrm;
layout(location = 2) in vec2 a_uv;
layout(location = 3) in vec4 a_tan;
#ifdef INSTANCED
layout(location = 4) in mat4 a_inst;
#endif
uniform mat4 u_viewProj;
uniform mat4 u_model;
out vec3 v_pos;
out vec3 v_nrm;
out vec2 v_uv;
out vec4 v_tan;
void main() {
#ifdef INSTANCED
    mat4 M = a_inst * u_model;
#else
    mat4 M = u_model;
#endif
    vec4 wp = M * vec4(a_pos, 1.0);
    mat3 A = mat3(M);
    mat3 NM = mat3(A[0] / dot(A[0], A[0]), A[1] / dot(A[1], A[1]), A[2] / dot(A[2], A[2]));
    v_pos = wp.xyz;
    v_nrm = normalize(NM * a_nrm);
    v_tan = vec4(normalize(A * a_tan.xyz), a_tan.w);
    v_uv = a_uv;
    gl_Position = u_viewProj * wp;
}
