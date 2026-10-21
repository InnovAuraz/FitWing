#version 330 core
in vec2 v_uv;
in vec4 v_color;
in vec4 v_misc;
in float v_dist;
out vec4 o_color;
uniform sampler2D u_noise;
uniform float u_fogDensity;
uniform float u_thermal;
uniform float u_thermalFog;
void main() {
    float r = length(v_uv);
    if (r > 1.0) discard;
    float type = v_misc.y;
    float a;
    if (type < 0.5) {            // soft smoke / dust puff
        vec2 nuv = v_uv * 0.35 + 0.5 + vec2(v_misc.x * 0.37, v_misc.x * 0.21);
        float n = texture(u_noise, nuv).g * 0.6 + texture(u_noise, nuv * 2.7).b * 0.4;
        a = smoothstep(1.0, 0.15, r) * (0.35 + 0.9 * n);
    } else if (type < 1.5) {     // fire / flash glow
        a = exp(-r * r * 3.2);
    } else if (type < 2.5) {     // star flare (LEDs, sparks)
        float core = exp(-r * r * 9.0);
        float cross_ = exp(-abs(v_uv.x) * 18.0) * exp(-abs(v_uv.y) * 2.5) + exp(-abs(v_uv.y) * 18.0) * exp(-abs(v_uv.x) * 2.5);
        a = core + 0.35 * cross_;
    } else {                     // hard dot (debris)
        a = smoothstep(1.0, 0.7, r);
    }
    float fogK = (u_thermal > 0.5) ? u_thermalFog : 1.0;
    float T = exp(-u_fogDensity * fogK * v_dist);
    vec3 col = v_color.rgb;
    float alpha = clamp(v_color.a * a, 0.0, 1.0) * T;
    if (u_thermal > 0.5) col = vec3(v_misc.z);
    o_color = vec4(col, alpha);
}
