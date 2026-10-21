#version 330 core
uniform mat4 u_viewProj;
uniform vec3 u_camPos;
uniform float u_time;
uniform float u_wind;
uniform float u_amount;   // 0..1 fraction of drops that are visible
out float v_a;
float h(float n) { return fract(sin(n * 12.9898) * 43758.5453); }
void main() {
    float fid = float(gl_InstanceID);
    const float L = 36.0, H = 28.0;
    vec3 rnd = vec3(h(fid * 1.7 + 0.3), h(fid * 2.3 + 1.1), h(fid * 3.1 + 2.7));
    float speed = 12.0 + 4.0 * rnd.z;
    vec3 vel = vec3(u_wind, -speed, u_wind * 0.35);
    float y = fract(rnd.y - u_time * speed / H) * H - H * 0.5;
    vec3 base = u_camPos + vec3((rnd.x - 0.5) * L, y, (h(fid * 5.9 + 4.4) - 0.5) * L);
    vec3 dir = normalize(vel);
    float len = 0.45 + 0.4 * rnd.x;
    vec3 p0 = base;
    vec3 p1 = base - dir * len;
    vec3 toCam = normalize(u_camPos - base);
    vec3 side = normalize(cross(dir, toCam)) * (0.005 + 0.0006 * length(base - u_camPos));
    int vid = gl_VertexID;
    vec3 p = ((vid & 2) == 0) ? p0 : p1;
    p += (((vid & 1) == 0) ? -side : side);
    v_a = (rnd.z < u_amount ? 1.0 : 0.0) * (0.35 + 0.4 * rnd.x) * (((vid & 2) == 0) ? 0.25 : 1.0);
    gl_Position = u_viewProj * vec4(p, 1.0);
}
