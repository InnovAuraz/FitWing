#version 330 core
layout(location = 0) in vec4 i_posSize;   // xyz, size (world radius)
layout(location = 1) in vec4 i_color;
layout(location = 2) in vec4 i_misc;      // rotation, type, heat, minPixels
uniform mat4 u_viewProj;
uniform vec3 u_camRight;
uniform vec3 u_camUp;
uniform vec3 u_camPos;
uniform float u_pixelAngle;   // radians per pixel
out vec2 v_uv;
out vec4 v_color;
out vec4 v_misc;
out float v_dist;
void main() {
    vec2 c = vec2(float(gl_VertexID & 1), float((gl_VertexID >> 1) & 1)) * 2.0 - 1.0;
    float cs = cos(i_misc.x), sn = sin(i_misc.x);
    vec2 r = vec2(c.x * cs - c.y * sn, c.x * sn + c.y * cs);
    float dist = length(i_posSize.xyz - u_camPos);
    float size = max(i_posSize.w, dist * u_pixelAngle * i_misc.w);
    vec3 wp = i_posSize.xyz + (u_camRight * r.x + u_camUp * r.y) * size;
    v_uv = c;
    v_color = i_color;
    v_misc = i_misc;
    v_dist = dist;
    gl_Position = u_viewProj * vec4(wp, 1.0);
}
