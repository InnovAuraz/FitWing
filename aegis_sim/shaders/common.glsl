// Shared lighting / fog / shadow code. Included by object, terrain, water shaders.
uniform vec3 u_camPos;
uniform vec3 u_keyDir;      // direction towards the key light (sun or moon)
uniform vec3 u_keyColor;    // HDR radiance
uniform vec3 u_ambSky;
uniform vec3 u_ambGround;
uniform vec3 u_fogColor;
uniform float u_fogDensity;
uniform float u_wet;
uniform float u_flash;      // lightning / blast flash 0..1
uniform float u_time;
uniform float u_thermal;    // 1.0 => write heat instead of colour
uniform float u_thermalFog; // fog density multiplier for thermal
uniform int u_numLights;
uniform vec3 u_lightPos[4];
uniform vec3 u_lightCol[4];
uniform float u_lightRange[4];
uniform mat4 u_shadowVP;
uniform sampler2DShadow u_shadowMap;
uniform float u_shadowOn;

const float PI = 3.14159265;

float shadowAt(vec3 wp, vec3 N) {
    if (u_shadowOn < 0.5) return 1.0;
    vec4 lp = u_shadowVP * vec4(wp + N * 0.05, 1.0);
    vec3 p = lp.xyz / lp.w * 0.5 + 0.5;
    if (p.x < 0.0 || p.x > 1.0 || p.y < 0.0 || p.y > 1.0 || p.z > 1.0) return 1.0;
    float s = 0.0;
    vec2 ts = vec2(1.0 / 2048.0);
    for (int x = -1; x <= 1; ++x)
        for (int y = -1; y <= 1; ++y)
            s += texture(u_shadowMap, vec3(p.xy + vec2(float(x), float(y)) * ts, p.z - 0.0012));
    float edge = smoothstep(0.0, 0.06, min(min(p.x, 1.0 - p.x), min(p.y, 1.0 - p.y)));
    return mix(1.0, s / 9.0, edge);
}

float D_GGX(float NoH, float a) {
    float a2 = a * a;
    float d = NoH * NoH * (a2 - 1.0) + 1.0;
    return a2 / (PI * d * d + 1e-6);
}
float G_Smith(float NoV, float NoL, float rough) {
    float k = (rough + 1.0) * (rough + 1.0) / 8.0;
    return (NoV / (NoV * (1.0 - k) + k)) * (NoL / (NoL * (1.0 - k) + k));
}
vec3 F_Schlick(vec3 f0, float c) { return f0 + (1.0 - f0) * pow(1.0 - c, 5.0); }

vec3 lightContribution(vec3 L, vec3 radiance, vec3 N, vec3 V, vec3 diff, vec3 f0, float rough, float NoV) {
    float NoL = max(dot(N, L), 0.0);
    if (NoL <= 0.0) return vec3(0.0);
    vec3 H = normalize(L + V);
    float NoH = max(dot(N, H), 0.0), VoH = max(dot(V, H), 0.0);
    vec3 F = F_Schlick(f0, VoH);
    float a = rough * rough;
    vec3 spec = F * (D_GGX(NoH, a) * G_Smith(NoV, NoL, rough) / (4.0 * NoL * NoV + 1e-4));
    return (diff * (1.0 - F) / PI + spec) * radiance * NoL;
}

vec3 shadePBR(vec3 wp, vec3 N, vec3 V, vec3 albedo, float rough, float metal, float ao, float sh) {
    rough = clamp(rough, 0.05, 1.0);
    vec3 f0 = mix(vec3(0.04), albedo, metal);
    vec3 diff = albedo * (1.0 - metal);
    float NoV = max(dot(N, V), 1e-3);
    vec3 col = lightContribution(normalize(u_keyDir), u_keyColor * sh, N, V, diff, f0, rough, NoV);
    for (int i = 0; i < 4; ++i) {
        if (i >= u_numLights) break;
        vec3 Lv = u_lightPos[i] - wp;
        float d = length(Lv);
        if (d >= u_lightRange[i]) continue;
        float w = clamp(1.0 - pow(d / u_lightRange[i], 4.0), 0.0, 1.0);
        float att = w * w / (d * d + 0.6);
        col += lightContribution(Lv / d, u_lightCol[i] * att, N, V, diff, f0, rough, NoV);
    }
    vec3 amb = mix(u_ambGround, u_ambSky, N.y * 0.5 + 0.5);
    vec3 R = reflect(-V, N);
    vec3 rc = mix(u_ambGround, u_ambSky, clamp(R.y * 0.5 + 0.5, 0.0, 1.0));
    vec3 Fa = f0 + (max(vec3(1.0 - rough), f0) - f0) * pow(1.0 - NoV, 5.0);
    col += (diff * amb + Fa * rc * (1.0 - rough * 0.75)) * ao;
    col += u_flash * vec3(0.55, 0.6, 0.85) * diff * ao * 0.5;
    return col;
}

vec3 applyFog(vec3 c, vec3 wp) {
    float d = length(wp - u_camPos);
    return mix(u_fogColor, c, exp(-u_fogDensity * d));
}
float heatFog(float h, vec3 wp) {
    float d = length(wp - u_camPos);
    return mix(0.04, h, exp(-u_fogDensity * u_thermalFog * d));
}
