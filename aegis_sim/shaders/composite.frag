#version 330 core
in vec2 v_uv;
out vec4 o_color;
uniform sampler2D u_scene;
uniform sampler2D u_bloom;
uniform float u_exposure;
uniform float u_bloomAmount;
uniform float u_time;
uniform float u_vignette;
uniform float u_grain;
uniform float u_flicker;     // 0..1 brightness flicker (FNAF style)
uniform float u_static;      // 0..1 static burst (item switch / damage)
uniform float u_saturation;
uniform vec3 u_shadowTint;
uniform vec2 u_texel;
uniform float u_aberration;

float hash21(vec2 p) {
    p = fract(p * vec2(123.34, 456.21));
    p += dot(p, p + 45.32);
    return fract(p.x * p.y);
}
vec3 aces(vec3 x) {
    const float a = 2.51, b = 0.03, c = 2.43, d = 0.59, e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}
void main() {
    vec2 uv = v_uv;
    // static tearing
    if (u_static > 0.01) {
        float band = floor(uv.y * 40.0);
        float tear = (hash21(vec2(band, floor(u_time * 24.0))) - 0.5) * 0.08 * u_static;
        uv.x += tear * step(0.6, hash21(vec2(band, 7.0 + floor(u_time * 24.0))));
    }
    vec2 cc = uv - 0.5;
    float ab = u_aberration * dot(cc, cc);
    vec3 c;
    c.r = texture(u_scene, uv + cc * ab).r;
    c.g = texture(u_scene, uv).g;
    c.b = texture(u_scene, uv - cc * ab).b;
    c += texture(u_bloom, uv).rgb * u_bloomAmount;
    c *= u_exposure * (1.0 - u_flicker);
    c = aces(c);
    float lum = dot(c, vec3(0.299, 0.587, 0.114));
    c = mix(vec3(lum), c, u_saturation);
    c += u_shadowTint * (1.0 - smoothstep(0.0, 0.35, lum));
    float vig = smoothstep(1.15, 0.35, length(cc * vec2(1.1, 1.0)) * 1.25);
    c *= mix(1.0, vig, u_vignette);
    float g = hash21(uv * vec2(1920.0, 1080.0) + fract(u_time) * 100.0) - 0.5;
    c += g * u_grain;
    if (u_static > 0.01) {
        float n = hash21(uv * vec2(640.0, 360.0) + floor(u_time * 30.0));
        c = mix(c, vec3(n * 0.8), clamp(u_static, 0.0, 1.0) * 0.85);
    }
    o_color = vec4(c, 1.0);
}
