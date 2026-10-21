#version 330 core
in vec2 v_uv;
out vec4 o_color;
uniform sampler2D u_img;
uniform sampler2D u_noise;
uniform int u_mode;          // 0 binoculars day, 1 night vision, 2 thermal, 3 scope / sight
uniform float u_time;
uniform float u_exposure;
uniform int u_degraded;
uniform vec2 u_texel;
uniform float u_aspect;
uniform float u_circle;      // 1: circular mask (rifle scope)
uniform int u_palette;       // thermal: 0 ironbow, 1 white-hot
uniform float u_gain;
uniform float u_static;
uniform float u_zoom;

float hash21(vec2 p) {
    p = fract(p * vec2(123.34, 456.21));
    p += dot(p, p + 45.32);
    return fract(p.x * p.y);
}
vec3 aces(vec3 x) {
    const float a = 2.51, b = 0.03, c = 2.43, d = 0.59, e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}
vec3 ironbow(float t) {
    t = clamp(t, 0.0, 1.0);
    vec3 c0 = vec3(0.0, 0.0, 0.03), c1 = vec3(0.20, 0.0, 0.42), c2 = vec3(0.72, 0.06, 0.26);
    vec3 c3 = vec3(0.98, 0.45, 0.04), c4 = vec3(1.0, 0.88, 0.25), c5 = vec3(1.0, 1.0, 0.95);
    if (t < 0.2) return mix(c0, c1, t / 0.2);
    if (t < 0.4) return mix(c1, c2, (t - 0.2) / 0.2);
    if (t < 0.6) return mix(c2, c3, (t - 0.4) / 0.2);
    if (t < 0.8) return mix(c3, c4, (t - 0.6) / 0.2);
    return mix(c4, c5, (t - 0.8) / 0.2);
}
vec3 blur9(vec2 uv, float rad) {
    vec3 s = vec3(0.0);
    for (int i = -1; i <= 1; ++i)
        for (int j = -1; j <= 1; ++j) s += texture(u_img, uv + vec2(float(i), float(j)) * u_texel * rad).rgb;
    return s / 9.0;
}

void main() {
    vec2 uv = v_uv;
    vec2 cc = uv - 0.5;
    vec2 ca = cc * vec2(u_aspect, 1.0);
    float r = length(ca);
    float t = u_time;
    vec3 col;

    if (u_mode == 2) {
        // ---- thermal imager --------------------------------------------------------------------
        vec2 suv = uv;
        if (u_degraded == 1) {
            vec2 cells = vec2(1.0 / (u_texel.x * 3.0), 1.0 / (u_texel.y * 3.0));
            suv = (floor(uv * cells) + 0.5) / cells;
        }
        float h = texture(u_img, suv).r;
        if (u_degraded == 1) {
            h += (hash21(floor(uv / u_texel / 3.0) + floor(t * 12.0)) - 0.5) * 0.10;
            h += 0.06 * sin(uv.y * 40.0 + t * 1.3) * smoothstep(0.4, 1.0, sin(t * 0.37));
            h += (texture(u_noise, uv * 8.0).g - 0.5) * 0.08;
            h = (floor(h * 14.0)) / 14.0;
        } else {
            h += (hash21(uv / u_texel + floor(t * 20.0)) - 0.5) * 0.018;
        }
        float tn = clamp((h - 0.10) / 0.85, 0.0, 1.0);
        tn = pow(tn, 0.9);
        col = (u_palette == 1) ? vec3(tn) : ironbow(tn);
        float scan = 0.94 + 0.06 * sin(uv.y / u_texel.y * 3.14159);
        col *= scan;
        col *= mix(1.0, smoothstep(0.95, 0.4, r), 0.55);
        if (u_degraded == 1) {
            float nuc = step(0.985, hash21(vec2(floor(t * 2.0), 3.0)));
            col = mix(col, vec3(0.25), nuc * 0.7);
        }
    } else if (u_mode == 1) {
        // ---- image-intensifier night vision -----------------------------------------------------
        vec3 c = texture(u_img, uv).rgb;
        vec3 halo = blur9(uv, 4.0);
        float lum = dot(c, vec3(0.3, 0.59, 0.11));
        float hl = dot(halo, vec3(0.3, 0.59, 0.11));
        float sig = (lum + 0.5 * hl) * u_gain;
        float nz = hash21(uv / u_texel + fract(t * 7.0) * 311.0) - 0.5;
        float nzs = hash21(uv / u_texel * 0.5 + fract(t * 11.0) * 71.0) - 0.5;
        float noiseAmp = (0.35 / (0.6 + sig * 2.0)) * (u_degraded == 1 ? 2.2 : 1.0);
        sig += (nz * 0.7 + nzs * 0.5) * noiseAmp * 0.25;
        if (u_degraded == 1) {
            float roll = smoothstep(0.0, 0.05, abs(fract(uv.y * 0.6 - t * 0.12) - 0.5) - 0.44) * 0.25;
            sig *= 0.8 + 0.2 * sin(t * 13.0);
            sig += roll;
        }
        float v = 1.0 - exp(-max(sig, 0.0) * 1.6);
        col = vec3(0.10, 1.0, 0.22) * v;
        col += vec3(0.4, 0.55, 0.4) * smoothstep(0.85, 1.0, v) * 0.25;
        col *= smoothstep(0.95, 0.30, r);
    } else {
        // ---- day optics (binoculars, rifle scope, launcher sight) -------------------------------
        float k = 0.0028 * (0.2 + r * 2.0);
        vec3 c;
        c.r = texture(u_img, uv + cc * k * 2.0).r;
        c.g = texture(u_img, uv).g;
        c.b = texture(u_img, uv - cc * k * 2.0).b;
        if (u_degraded == 1) {
            float rad = 2.0 + 2.5 * (0.5 + 0.5 * sin(t * 0.6)) + 1.5 * step(0.97, hash21(vec2(floor(t * 3.0), 9.0)));
            c = mix(c, blur9(uv, rad), 0.85);
            float l = dot(c, vec3(0.333));
            c = mix(c, vec3(l), 0.35) * 0.8 + 0.012;
            c *= 0.88 + 0.12 * sin(t * 9.0 + 1.0);
        }
        c *= u_exposure;
        col = aces(c);
        col = pow(col, vec3(0.97));
        float vig = smoothstep(0.98, 0.42, r);
        col *= mix(0.45, 1.0, vig);
        // lens dust
        float dust = step(0.93, texture(u_noise, uv * vec2(u_aspect, 1.0) * 5.0).g) * 0.06;
        col += dust * vec3(0.8, 0.85, 0.9) * (1.0 - vig * 0.7);
        col += (hash21(uv / u_texel + fract(t) * 91.0) - 0.5) * (u_degraded == 1 ? 0.07 : 0.015);
    }
    if (u_static > 0.01) {
        float n = hash21(uv * vec2(480.0, 270.0) + floor(t * 30.0));
        col = mix(col, vec3(n), clamp(u_static, 0.0, 1.0) * 0.9);
    }
    float alpha = 1.0;
    if (u_circle > 0.5) {
        float rr = length(cc * vec2(u_aspect, 1.0)) / 0.5;
        alpha = 1.0 - smoothstep(0.985, 1.0, rr);
    }
    o_color = vec4(col, alpha);
}
