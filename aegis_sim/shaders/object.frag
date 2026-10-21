#version 330 core
#include "common.glsl"
in vec3 v_pos;
in vec3 v_nrm;
in vec2 v_uv;
in vec4 v_tan;
out vec4 o_color;

uniform sampler2D u_albedoTex;
uniform sampler2D u_normalTex;
uniform sampler2D u_ormTex;
uniform sampler2D u_emissiveTex;
uniform vec4 u_baseColor;
uniform float u_metallic;
uniform float u_roughness;
uniform float u_normalStrength;
uniform float u_aoFromOrm;
uniform float u_procedural;   // 1: procedural material (blend factors), 0: glTF (multiply factors)
uniform vec3 u_emissiveFactor;
uniform float u_alphaMask;
uniform float u_alphaCutoff;
uniform float u_heat;
uniform float u_emissiveBoost;   // e.g. dim windows during the day
uniform float u_heatAmbient;     // ambient scene temperature contribution

void main() {
    vec4 tex = texture(u_albedoTex, v_uv);
    float alpha = tex.a * u_baseColor.a;
    if (u_alphaMask > 0.5 && alpha < u_alphaCutoff) discard;
    vec3 albedo = tex.rgb * u_baseColor.rgb;

    vec3 N = normalize(v_nrm);
    if (!gl_FrontFacing) N = -N;
    vec3 T = normalize(v_tan.xyz - N * dot(N, v_tan.xyz));
    vec3 B = cross(N, T) * v_tan.w;
    vec3 nt = texture(u_normalTex, v_uv).xyz * 2.0 - 1.0;
    nt.xy *= u_normalStrength;
    N = normalize(T * nt.x + B * nt.y + N * max(nt.z, 0.05));

    vec3 orm = texture(u_ormTex, v_uv).rgb;
    float ao = u_aoFromOrm > 0.5 ? orm.r : 1.0;
    float rough, metal;
    if (u_procedural > 0.5) {
        ao = orm.r;
        rough = mix(u_roughness, orm.g, 0.5);
        metal = max(u_metallic, orm.b);
    } else {
        rough = orm.g * u_roughness;
        metal = orm.b * u_metallic;
    }
    vec3 emissive = texture(u_emissiveTex, v_uv).rgb * u_emissiveFactor * u_emissiveBoost;

    if (u_thermal > 0.5) {
        float lum = dot(albedo, vec3(0.3, 0.59, 0.11));
        float h = u_heat * (0.88 + 0.24 * clamp(lum * 2.0, 0.0, 1.0)) + u_heatAmbient + dot(emissive, vec3(0.33)) * 0.18;
        h = heatFog(h, v_pos);
        o_color = vec4(h, h, h, 1.0);
        return;
    }

    albedo *= 1.0 - 0.3 * u_wet;
    rough = mix(rough, 0.25, u_wet * 0.5);
    vec3 V = normalize(u_camPos - v_pos);
    float sh = shadowAt(v_pos, normalize(v_nrm));
    vec3 col = shadePBR(v_pos, N, V, albedo, rough, metal, ao, sh) + emissive;
    col = applyFog(col, v_pos);
    o_color = vec4(col, 1.0);
}
