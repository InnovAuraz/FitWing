#version 330 core
#include "common.glsl"
#include "sky_common.glsl"
in vec3 v_pos;
in float v_depth;
out vec4 o_color;
uniform sampler2D u_waterN;
uniform sampler2D u_noise;
uniform float u_waveAmp;   // wave steepness (storm => bigger)

void main() {
    if (v_depth < -0.15) discard;
    vec2 p = v_pos.xz;
    float t = u_time;
    float dist = length(v_pos - u_camPos);
    float k = 1.0 - smoothstep(60.0, 600.0, dist) * 0.75;
    vec3 n1 = texture(u_waterN, p * 0.045 + vec2(t * 0.012, t * 0.007)).xyz * 2.0 - 1.0;
    vec3 n2 = texture(u_waterN, p * 0.13 + vec2(-t * 0.02, t * 0.015)).xyz * 2.0 - 1.0;
    vec3 n3 = texture(u_waterN, p * 0.5 + vec2(t * 0.05, -t * 0.03)).xyz * 2.0 - 1.0;
    vec3 nt = n1 * 0.9 + n2 * 0.7 + n3 * 0.35 * k;
    nt.xy *= u_waveAmp * k;
    nt = normalize(vec3(nt.xy, 1.0));
    vec3 N = normalize(vec3(nt.x, nt.z, -nt.y));  // tangent frame: T=+x, up=+y
    vec3 V = normalize(u_camPos - v_pos);
    float NoV = max(dot(N, V), 0.02);
    float F = 0.02 + 0.98 * pow(1.0 - NoV, 5.0);

    vec3 R = reflect(-V, N);
    R.y = abs(R.y);
    vec3 refl = skyBase(R) + u_keyColor * 0.0;
    float depth = max(v_depth, 0.0);
    vec3 deep = vec3(0.012, 0.05, 0.07);
    vec3 shallow = vec3(0.07, 0.22, 0.20);
    vec3 body = mix(shallow, deep, smoothstep(0.0, 14.0, depth));
    // lit body colour from ambient + key
    float kl = max(dot(vec3(0, 1, 0), normalize(u_keyDir)), 0.0);
    vec3 lit = body * (u_ambSky * 1.2 + u_keyColor * (0.25 + 0.5 * kl));
    // specular glint
    vec3 L = normalize(u_keyDir);
    vec3 H = normalize(L + V);
    float NoH = max(dot(N, H), 0.0);
    float spec = D_GGX(NoH, 0.05) * max(dot(N, L), 0.0) * 0.08;
    vec3 col = mix(lit, refl, F) + u_keyColor * spec * F_Schlick(vec3(0.02), max(dot(V, H), 0.0));

    // foam at shoreline and on wave crests
    float f1 = texture(u_noise, p * 0.11 + vec2(t * 0.02, 0.0)).r;
    float foam = smoothstep(1.4, 0.0, depth) * smoothstep(0.35, 0.8, f1 + 0.25 * sin(depth * 3.0 - t * 1.4));
    foam += smoothstep(0.7, 0.98, texture(u_noise, p * 0.09 + t * 0.01).g) * u_waveAmp * 0.07 * k;
    col = mix(col, vec3(0.8, 0.85, 0.9) * (u_ambSky * 1.5 + u_keyColor * 0.35 * kl + 0.02), clamp(foam, 0.0, 1.0) * 0.85);

    if (u_thermal > 0.5) {
        float h = 0.20 + 0.05 * foam;
        h = heatFog(h, v_pos);
        o_color = vec4(h, h, h, 1.0);
        return;
    }
    col = applyFog(col, v_pos);
    float alpha = smoothstep(-0.12, 0.35, v_depth);
    o_color = vec4(col, alpha);
}
