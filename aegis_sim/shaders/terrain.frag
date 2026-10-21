#version 330 core
#include "common.glsl"
in vec3 v_pos;
in vec3 v_nrm;
out vec4 o_color;

uniform sampler2D u_grassA;
uniform sampler2D u_grassN;
uniform sampler2D u_dirtA;
uniform sampler2D u_dirtN;
uniform sampler2D u_rockA;
uniform sampler2D u_rockN;
uniform sampler2D u_sandA;
uniform sampler2D u_sandN;
uniform sampler2D u_roadA;
uniform sampler2D u_roadN;
uniform sampler2D u_noise;
uniform int u_terrainType;   // 0 rural, 1 urban, 2 forest, 3 coastal
uniform vec2 u_coastDir;
uniform float u_seaLevel;
uniform float u_hasSea;
uniform float u_dayWarm;

// x = asphalt/road mask, y = sidewalk mask, z = lane dash mask.  Mirrors Terrain::roadInfo() in C++.
vec3 roadInfo(vec2 p) {
    float m = 0.0, side = 0.0, dash = 0.0;
    if (u_terrainType == 0) {
        float xc = 120.0 + 55.0 * sin(p.y / 310.0) + 20.0 * sin(p.y / 97.0 + 1.0);
        m = 1.0 - smoothstep(2.6, 3.8, abs(p.x - xc));
    } else if (u_terrainType == 1) {
        float dx = abs(mod(p.x, 130.0) - 65.0);
        float dz = abs(mod(p.y, 130.0) - 65.0);
        float dr = min(dx, dz);
        m = 1.0 - smoothstep(6.0, 7.2, dr);
        side = smoothstep(6.0, 7.2, dr) * (1.0 - smoothstep(8.6, 9.6, dr));
        if (dx < dz) dash = step(abs(dx), 0.13) * step(fract(p.y / 9.0), 0.55) * step(8.0, dz);
        else dash = step(abs(dz), 0.13) * step(fract(p.x / 9.0), 0.55) * step(8.0, dx);
    } else if (u_terrainType == 2) {
        float xc = 70.0 + 35.0 * sin(p.y / 180.0) + 14.0 * sin(p.y / 61.0 + 2.0);
        m = 1.0 - smoothstep(1.4, 2.2, abs(p.x - xc));
    } else {
        vec2 perp = vec2(-u_coastDir.y, u_coastDir.x);
        float s = dot(p, perp);
        float dd = abs(dot(p, u_coastDir) - 70.0 + 6.0 * sin(s / 140.0));
        m = 1.0 - smoothstep(2.8, 3.8, dd);
        dash = step(abs(dd), 0.13) * step(fract(s / 9.0), 0.55);
    }
    return vec3(m, side, dash * m);
}

// Breaks visible tile repetition by blending two differently scaled / rotated samples.
vec4 aniTex(sampler2D t, vec2 uv) {
    vec2 uv2 = mat2(0.8, 0.6, -0.6, 0.8) * uv * 0.37 + vec2(0.31, 0.57);
    return 0.5 * (texture(t, uv) + texture(t, uv2));
}

void main() {
    vec3 N0 = normalize(v_nrm);
    vec2 p = v_pos.xz;
    float dist = length(v_pos - u_camPos);

    vec4 nz = texture(u_noise, p * 0.0021);
    vec4 nz2 = texture(u_noise, p * 0.0097);
    vec4 nz3 = texture(u_noise, p * 0.043);

    float slope = 1.0 - N0.y;
    float wRock = smoothstep(0.10, 0.26, slope + (nz.g - 0.5) * 0.10);
    float wSand = u_hasSea * (1.0 - smoothstep(u_seaLevel + 1.2, u_seaLevel + 5.0, v_pos.y + (nz2.r - 0.5) * 2.5));
    wSand *= 1.0 - wRock * 0.6;
    float wDirt = smoothstep(0.62, 0.78, nz2.b) * 0.85 * (1.0 - wRock);
    if (u_terrainType == 2) wDirt = max(wDirt, smoothstep(0.45, 0.7, nz2.g) * 0.7 * (1.0 - wRock));
    float wGrass = max(0.0, 1.0 - wRock - wSand - wDirt);
    float wsum = wRock + wSand + wDirt + wGrass + 1e-4;
    wRock /= wsum; wSand /= wsum; wDirt /= wsum; wGrass /= wsum;

    vec3 ri = roadInfo(p);

    vec2 uvA = p * 0.28;
    vec2 uvB = p * 0.037;
    float far = smoothstep(40.0, 260.0, dist);
    float macro = 0.7 + 0.6 * nz.r;
    float macro2 = 0.85 + 0.3 * nz3.r;

    vec3 gA = aniTex(u_grassA, uvA).rgb * mix(1.0, 0.8 + 0.4 * texture(u_grassA, uvB).g, far);
    vec3 dA = aniTex(u_dirtA, uvA).rgb;
    vec3 rA = aniTex(u_rockA, uvA * 0.8).rgb;
    vec3 sA = aniTex(u_sandA, uvA * 0.7).rgb;
    vec3 aA = texture(u_roadA, p * 0.35).rgb;

    vec3 alb = gA * wGrass * macro * macro2 + dA * wDirt * macro2 + rA * wRock + sA * wSand;

    // Rural crop fields
    if (u_terrainType == 0) {
        float field = smoothstep(0.52, 0.58, nz.r) * (1.0 - wRock) * (1.0 - wDirt);
        float rows = 0.5 + 0.5 * sin(dot(p, vec2(0.6, 0.8)) * 3.9);
        float rows2 = 0.5 + 0.5 * sin(dot(p, vec2(-0.8, 0.6)) * 3.3);
        vec3 crop = mix(vec3(0.24, 0.21, 0.045), vec3(0.09, 0.20, 0.04), smoothstep(0.35, 0.65, nz.b));
        float rowFade = 1.0 - smoothstep(25.0, 140.0, length(v_pos - u_camPos));
        crop *= 0.88 + 0.22 * mix(0.5, mix(rows, rows2, step(0.5, nz.g)), rowFade);
        alb = mix(alb, crop * macro2, field * 0.85 * wGrass);
    }
    // Roads
    vec3 roadAlb;
    if (u_terrainType == 1 || u_terrainType == 3) roadAlb = aA;
    else roadAlb = dA * vec3(1.25, 1.15, 1.0) * (0.8 + 0.4 * nz3.g);
    alb = mix(alb, roadAlb, ri.x);
    vec3 walk = aA * 3.2 + vec3(0.03);
    alb = mix(alb, walk, ri.y);
    alb = mix(alb, vec3(0.55, 0.48, 0.12), ri.z);

    // tangent-space normal blend (fades with distance)
    vec3 nG = aniTex(u_grassN, uvA).xyz * 2.0 - 1.0;
    vec3 nD = aniTex(u_dirtN, uvA).xyz * 2.0 - 1.0;
    vec3 nR = aniTex(u_rockN, uvA * 0.8).xyz * 2.0 - 1.0;
    vec3 nS = aniTex(u_sandN, uvA * 0.7).xyz * 2.0 - 1.0;
    vec3 nA = texture(u_roadN, p * 0.35).xyz * 2.0 - 1.0;
    vec3 nt = nG * wGrass + nD * wDirt + nR * wRock + nS * wSand;
    float rw = max(ri.x, ri.y);
    nt = mix(nt, nA, rw);
    nt = normalize(nt + vec3(0.0, 0.0, 1e-3));
    nt = normalize(mix(nt, vec3(0, 0, 1), far * 0.85));
    vec3 T = normalize(vec3(1.0, 0.0, 0.0) - N0 * N0.x);
    vec3 B = cross(N0, T);
    vec3 N = normalize(T * nt.x + B * nt.y + N0 * nt.z);

    float rough = 0.92 * wGrass + 0.95 * wDirt + 0.85 * wRock + 0.95 * wSand;
    rough = mix(rough, 0.82, ri.x);

    if (u_thermal > 0.5) {
        float h = 0.26 + 0.14 * u_dayWarm + 0.06 * nz3.g + 0.05 * (1.0 - wRock) * nz2.r;
        h += ri.x * 0.10 * (0.4 + u_dayWarm) + wRock * 0.04 * u_dayWarm;
        if (u_terrainType == 3) h = mix(h, 0.22, wSand * 0.3);
        h *= 1.0 - 0.25 * u_wet;
        h = heatFog(h, v_pos);
        o_color = vec4(h, h, h, 1.0);
        return;
    }

    float wetMask = u_wet * (0.55 + 0.45 * ri.x);
    float puddle = u_wet * smoothstep(0.52, 0.38, nz3.g) * smoothstep(0.03, 0.0, slope) * (1.0 - wRock);
    alb *= 1.0 - 0.38 * wetMask;
    rough = mix(rough, 0.28, wetMask * 0.6);
    rough = mix(rough, 0.04, puddle);
    alb *= 1.0 - 0.4 * puddle;
    N = normalize(mix(N, N0, puddle));

    vec3 V = normalize(u_camPos - v_pos);
    float sh = shadowAt(v_pos, N0);
    vec3 col = shadePBR(v_pos, N, V, alb, rough, 0.0, 1.0, sh);
    col = applyFog(col, v_pos);
    o_color = vec4(col, 1.0);
}
