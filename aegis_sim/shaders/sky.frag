#version 330 core
#include "sky_common.glsl"
in vec2 v_ndc;
out vec4 o_color;
uniform mat4 u_invViewRotProj;
uniform sampler2D u_noise;
uniform float u_time;
uniform float u_cloud;      // coverage 0..1
uniform float u_storm;      // 0..1 darkness
uniform float u_stars;      // 0..1
uniform float u_flash;
uniform vec3 u_fogColor;
uniform float u_fogDensity;
uniform float u_thermal;
uniform vec3 u_keyColor;

float hash13(vec3 p) {
    p = fract(p * 0.1031);
    p += dot(p, p.zyx + 31.32);
    return fract((p.x + p.y) * p.z);
}

float cloudField(vec2 uv) {
    float n = 0.50 * texture(u_noise, uv * 0.21).r + 0.30 * texture(u_noise, uv * 0.53 + 0.31).g +
              0.20 * texture(u_noise, uv * 1.37 + 0.77).b;
    return n;
}

void main() {
    vec4 w = u_invViewRotProj * vec4(v_ndc, 1.0, 1.0);
    vec3 d = normalize(w.xyz / w.w);
    float h = d.y;

    float dens = 0.0;
    vec3 col = skyBase(d);
    if (h > 0.0) {
        vec2 cuv = d.xz / (h + 0.12) * 0.55 + vec2(u_time * 0.004, u_time * 0.0015);
        float n = cloudField(cuv);
        float thr = 1.0 - u_cloud;
        dens = smoothstep(thr - 0.02, thr + 0.28, n) * smoothstep(0.0, 0.12, h);
    }
    if (u_thermal > 0.5) {
        float hh = 0.03 + 0.07 * dens;
        o_color = vec4(hh, hh, hh, 1.0);
        return;
    }
    // stars
    if (u_stars > 0.01 && h > 0.0) {
        vec3 p = d * 170.0;
        vec3 id = floor(p);
        vec3 f = fract(p) - 0.5;
        float r = hash13(id);
        float star = step(0.988, r) * smoothstep(0.22, 0.0, length(f));
        float tw = 0.7 + 0.3 * sin(u_time * (2.0 + r * 5.0) + r * 40.0);
        vec3 sc = mix(vec3(0.7, 0.8, 1.0), vec3(1.0, 0.9, 0.7), hash13(id + 3.0));
        col += sc * star * tw * 1.6 * u_stars * (1.0 - dens);
        // milky way band
        float band = exp(-pow(dot(d, normalize(vec3(0.4, 0.8, 0.45))) * 3.2, 2.0));
        col += vec3(0.05, 0.06, 0.09) * band * texture(u_noise, d.xz * 2.0 + 0.3).r * u_stars;
    }
    // sun disc
    float sd = dot(d, u_sunDir);
    if (u_sunDir.y > -0.05) col += vec3(1.0, 0.92, 0.75) * smoothstep(0.99955, 0.99985, sd) * 40.0 * (1.0 - 0.9 * u_overcast) * (1.0 - dens);
    // moon
    float md = dot(d, u_moonDir);
    if (u_moonDir.y > 0.0) {
        float disc = smoothstep(0.99989, 0.99993, md);
        vec2 mp = vec2(dot(d - u_moonDir * md, normalize(cross(u_moonDir, vec3(0, 1, 0)))), d.y - u_moonDir.y * md) * 70.0;
        float surf = 0.72 + 0.28 * texture(u_noise, mp * 0.5 + 0.5).g;
        col += vec3(0.85, 0.9, 1.0) * disc * surf * 3.2 * (1.0 - dens * 0.9) * u_stars;
        col += vec3(0.25, 0.3, 0.45) * (pow(max(md, 0.0), 90.0) * 0.12 + pow(max(md, 0.0), 12.0) * 0.015) * u_stars;
    }
    // clouds
    if (dens > 0.0) {
        vec3 lit = u_horizon * 1.15 + u_zenith * 0.4 + u_glow * (0.25 + 0.6 * pow(max(sd, 0.0), 3.0)) * (1.0 - u_overcast);
        lit += u_keyColor * 0.06 * (1.0 - u_storm);
        float lum = dot(lit, vec3(0.333));
        vec3 dark = mix(lit, vec3(lum) * 0.4, 0.5 + 0.3 * u_storm) * (0.65 - 0.35 * u_storm);
        vec3 cc = mix(lit, dark, clamp(dens * 0.9 + u_storm * 0.5, 0.0, 1.0));
        col = mix(col, cc, clamp(dens * (0.65 + 0.35 * u_cloud + 0.2 * u_storm), 0.0, 1.0));
    }
    col += vec3(0.55, 0.6, 0.95) * u_flash * (0.25 + 1.2 * dens);
    // atmospheric fog band at the horizon
    float fogH = clamp(u_fogDensity * 80.0, 0.02, 1.2);
    float fogAmt = exp(-max(h, 0.0) / fogH * 2.0);
    col = mix(col, u_fogColor, fogAmt);
    o_color = vec4(col, 1.0);
}
