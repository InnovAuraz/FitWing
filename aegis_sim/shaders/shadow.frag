#version 330 core
in vec2 v_uv;
uniform sampler2D u_albedoTex;
uniform float u_alphaMask;
uniform float u_alphaCutoff;
uniform float u_alphaBase;
void main() {
    if (u_alphaMask > 0.5 && texture(u_albedoTex, v_uv).a * u_alphaBase < u_alphaCutoff) discard;
}
