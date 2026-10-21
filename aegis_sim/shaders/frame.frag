#version 330 core
in vec2 v_uv;
out vec4 o_color;
uniform vec4 u_outer;        // x0,y0,x1,y1 (pixels, origin bottom-left)
uniform vec4 u_hole;
uniform float u_cornerOuter;
uniform float u_cornerHole;
uniform vec3 u_tone;
uniform int u_style;         // 0 binoculars/camera, 1 thermal, 2 tablet, 3 scope ring
uniform float u_time;
uniform float u_blackout;    // 1: darken everything outside the device (scope tube)
uniform float u_alpha;
uniform sampler2D u_noise;
uniform float u_ledState;    // 0 off, 1 on (blinking rec LED / power)

float sdRoundBox(vec2 p, vec2 b, float r) {
    vec2 q = abs(p) - b + r;
    return length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - r;
}
float screw(vec2 p, vec2 c, float rad, out float slot) {
    vec2 d = p - c;
    float l = length(d);
    float a = 0.6;
    vec2 rd = vec2(d.x * cos(a) - d.y * sin(a), d.x * sin(a) + d.y * cos(a));
    slot = 1.0 - smoothstep(0.6, 1.4, abs(rd.y)) * step(abs(rd.x), rad * 0.8);
    return 1.0 - smoothstep(rad - 1.0, rad, l);
}

void main() {
    vec2 frag = gl_FragCoord.xy;
    vec2 oc = 0.5 * (u_outer.xy + u_outer.zw), oh = 0.5 * (u_outer.zw - u_outer.xy);
    vec2 hc = 0.5 * (u_hole.xy + u_hole.zw), hh = 0.5 * (u_hole.zw - u_hole.xy);
    float dO = sdRoundBox(frag - oc, oh, u_cornerOuter);
    float dH = sdRoundBox(frag - hc, hh, u_cornerHole);

    if (dO > 0.0) {
        o_color = vec4(0.0, 0.0, 0.0, u_blackout * u_alpha);
        return;
    }
    if (dH < 0.0) {
        o_color = vec4(0.0);
        return;
    }
    float depth = -dO;
    vec3 col = u_tone;
    vec2 g = normalize(vec2(dFdx(dO), dFdy(dO)) + 1e-6);
    vec2 L2 = normalize(vec2(-0.55, 0.85));

    // brushed / textured base
    float n = texture(u_noise, vec2(frag.x * 0.0016, frag.y * 0.40)).r;
    float n2 = texture(u_noise, frag * 0.011).g;
    col *= 0.78 + 0.34 * n;
    col *= 0.92 + 0.16 * n2;

    // rubber grips on the sides (binoculars / thermal / tablet)
    float gripW = (u_style == 3) ? 0.0 : oh.x * 0.085;
    float sideDist = oh.x - abs(frag.x - oc.x);
    if (gripW > 0.0 && sideDist < gripW && u_style != 3) {
        float dots = 0.5 + 0.5 * cos(frag.x * 0.85) * cos(frag.y * 0.85);
        col = u_tone * 0.28 * (0.75 + 0.35 * dots);
        float edge = smoothstep(gripW - 3.0, gripW, sideDist);
        col = mix(col, u_tone * 0.5, 1.0 - edge) * (0.9 + 0.1 * edge);
    }
    // knurled ring for the scope
    if (u_style == 3) {
        float ang = atan(frag.y - oc.y, frag.x - oc.x);
        float kn = 0.5 + 0.5 * cos(ang * 90.0);
        float ringBand = smoothstep(14.0, 18.0, depth) * (1.0 - smoothstep(oh.x * 0.10, oh.x * 0.10 + 4.0, depth));
        col = mix(col, u_tone * (0.45 + 0.35 * kn), ringBand);
    }
    // outer bevel
    float bev = 1.0 - smoothstep(0.0, 9.0, depth);
    col += bev * (0.30 * dot(g, L2) + 0.03);
    float rim = 1.0 - smoothstep(0.0, 1.5, depth);
    col = mix(col, vec3(0.02), rim * 0.65);

    // recessed bezel around the screen
    float ring = 1.0 - smoothstep(0.0, 18.0, dH);
    col *= mix(1.0, 0.22, ring);
    float lip = smoothstep(18.0, 19.0, dH) * (1.0 - smoothstep(20.0, 22.0, dH));
    col += lip * 0.12 * (0.6 + 0.4 * dot(-g, L2));
    float lipOuter = 1.0 - smoothstep(0.0, 2.0, abs(dH - 2.0));
    col = mix(col, vec3(0.01), lipOuter * 0.7);

    if (u_style != 3) {
        // corner screws
        float slot;
        float inset = 26.0;
        vec2 sp[4] = vec2[4](vec2(u_outer.x + inset, u_outer.y + inset), vec2(u_outer.z - inset, u_outer.y + inset),
                             vec2(u_outer.x + inset, u_outer.w - inset), vec2(u_outer.z - inset, u_outer.w - inset));
        for (int i = 0; i < 4; ++i) {
            float s = screw(frag, sp[i], 7.0, slot);
            if (s > 0.0) {
                vec3 sc = vec3(0.50, 0.50, 0.52) * (0.65 + 0.35 * dot(normalize(frag - sp[i] + 1e-4), L2));
                sc *= mix(0.25, 1.0, slot);
                col = mix(col, sc, s);
            }
            float sh = 1.0 - smoothstep(7.0, 11.0, length(frag - sp[i]));
            col *= 1.0 - 0.35 * sh * (1.0 - s);
        }
        // control buttons under the screen
        float mb = u_hole.y - u_outer.y;
        for (int i = -1; i <= 1; ++i) {
            vec2 bc = vec2(oc.x + float(i) * mb * 1.15, u_outer.y + mb * 0.45);
            float db = sdRoundBox(frag - bc, vec2(mb * 0.26, mb * 0.15), mb * 0.08);
            float inside = 1.0 - smoothstep(-1.0, 0.5, db);
            vec3 bcol = u_tone * (0.55 + 0.3 * (1.0 - smoothstep(0.0, mb * 0.2, -db)));
            col = mix(col, bcol, inside);
            col *= 1.0 - 0.4 * (1.0 - smoothstep(0.0, 5.0, db)) * (1.0 - inside);
        }
        // status LED
        vec2 lc = vec2(u_outer.x + (u_hole.x - u_outer.x) * 0.5, u_outer.w - (u_outer.w - u_hole.w) * 0.5);
        float ld = length(frag - lc);
        float led = 1.0 - smoothstep(5.0, 7.0, ld);
        vec3 ledCol = (u_style == 1) ? vec3(0.1, 1.0, 0.2) : vec3(1.0, 0.1, 0.05);
        float on = u_ledState * (0.5 + 0.5 * step(0.0, sin(u_time * 3.2)));
        col = mix(col, ledCol * (0.25 + 1.6 * on), led);
        col += ledCol * on * 0.5 * exp(-ld * 0.12) * step(7.0, ld) * 0.4;
    }
    // grime
    col *= 1.0 - 0.18 * step(0.93, texture(u_noise, frag * 0.05).g);
    o_color = vec4(max(col, vec3(0.0)), u_alpha);
}
