#version 330 core
in vec4 v_col;
in vec3 v_pos;
out vec4 o_color;
uniform vec3 u_camPos;
uniform vec3 u_fogColor;
uniform float u_fogDensity;
uniform float u_thermal;
uniform float u_thermalFog;
uniform float u_additive;
void main() {
    float d = length(v_pos - u_camPos);
    float T = exp(-u_fogDensity * (u_thermal > 0.5 ? u_thermalFog : 1.0) * d);
    vec3 c = v_col.rgb;
    if (u_thermal > 0.5) c = vec3(u_additive > 0.5 ? 1.0 : 0.3);
    else if (u_additive < 0.5) c = mix(u_fogColor, c, T);
    float a = v_col.a * (u_additive > 0.5 ? T : 1.0);
    o_color = vec4(c, a);
}
