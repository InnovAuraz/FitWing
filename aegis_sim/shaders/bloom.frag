#version 330 core
in vec2 v_uv;
out vec4 o_color;
uniform sampler2D u_tex;
uniform vec2 u_texel;
uniform vec2 u_dir;
uniform float u_threshold;
void main() {
#ifdef DOWN
    vec3 c = vec3(0.0);
    c += texture(u_tex, v_uv + u_texel * vec2(-1.0, -1.0)).rgb;
    c += texture(u_tex, v_uv + u_texel * vec2(1.0, -1.0)).rgb;
    c += texture(u_tex, v_uv + u_texel * vec2(-1.0, 1.0)).rgb;
    c += texture(u_tex, v_uv + u_texel * vec2(1.0, 1.0)).rgb;
    c *= 0.25;
    float l = max(c.r, max(c.g, c.b));
    float k = max(l - u_threshold, 0.0) / max(l, 1e-4);
    o_color = vec4(min(c * k, vec3(60.0)), 1.0);
#else
    vec3 c = texture(u_tex, v_uv).rgb * 0.2270270270;
    c += texture(u_tex, v_uv + u_dir * u_texel * 1.3846153846).rgb * 0.3162162162;
    c += texture(u_tex, v_uv - u_dir * u_texel * 1.3846153846).rgb * 0.3162162162;
    c += texture(u_tex, v_uv + u_dir * u_texel * 3.2307692308).rgb * 0.0702702703;
    c += texture(u_tex, v_uv - u_dir * u_texel * 3.2307692308).rgb * 0.0702702703;
    o_color = vec4(c, 1.0);
#endif
}
