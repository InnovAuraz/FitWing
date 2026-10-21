uniform vec3 u_zenith;
uniform vec3 u_horizon;
uniform vec3 u_glow;
uniform vec3 u_sunDir;
uniform vec3 u_moonDir;
uniform float u_overcast;

vec3 skyBase(vec3 d) {
    float h = clamp(d.y, -1.0, 1.0);
    float t = pow(clamp(h, 0.0, 1.0), 0.42);
    vec3 c = mix(u_horizon, u_zenith, t);
    float sd = max(dot(d, u_sunDir), 0.0);
    c += u_glow * (pow(sd, 6.0) * 0.35 + pow(sd, 48.0) * 0.8) * (1.0 - 0.85 * u_overcast);
    if (h < 0.0) c = mix(c, u_horizon * 0.55, clamp(-h * 6.0, 0.0, 1.0));
    return c;
}
