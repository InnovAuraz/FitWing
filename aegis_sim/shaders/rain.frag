#version 330 core
in float v_a;
out vec4 o_color;
uniform vec3 u_rainColor;
uniform float u_thermal;
void main() {
    if (u_thermal > 0.5) discard;
    o_color = vec4(u_rainColor, v_a * 0.5);
}
