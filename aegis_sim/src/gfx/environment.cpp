#include "gfx/environment.h"

namespace aegis {

static vec3 mixv(const vec3& a, const vec3& b, float t) { return a + (b - a) * saturate(t); }

float effectiveVisibility(const SceneDef& s) {
    float cap = 20000.0f;
    switch (s.weather) {
        case Weather::Clear: cap = 20000.0f; break;
        case Weather::Cloudy: cap = 12000.0f; break;
        case Weather::Rain: cap = 3200.0f; break;
        case Weather::Fog: cap = 700.0f; break;
        case Weather::Storm: cap = 2000.0f; break;
    }
    return std::min(cap, visibilityMeters(s.visibility));
}

Environment makeEnvironment(const SceneDef& s, uint32_t seed) {
    Environment e;
    Rng rng(seed ^ 0xA5F00Du);
    float az = rng.uniform(0.0f, kTwoPi);
    e.sunAzimuth = az;

    float sunElevDeg = 55.0f;
    switch (s.time) {
        case TimeOfDay::Day: sunElevDeg = rng.uniform(42.0f, 62.0f); break;
        case TimeOfDay::Twilight: sunElevDeg = rng.uniform(1.5f, 4.0f); break;
        case TimeOfDay::Night: sunElevDeg = -32.0f; break;
    }
    float se = radians(sunElevDeg);
    e.sunDir = vec3(std::sin(az) * std::cos(se), std::sin(se), -std::cos(az) * std::cos(se));
    e.sunElev = std::sin(se);
    float maz = az + kPi + rng.uniform(-0.8f, 0.8f);
    float me = radians(rng.uniform(32.0f, 46.0f));
    e.moonDir = vec3(std::sin(maz) * std::cos(me), std::sin(me), -std::cos(maz) * std::cos(me));

    // weather -----------------------------------------------------------------------------------
    switch (s.weather) {
        case Weather::Clear: e.cloud = 0.18f; e.overcast = 0.0f; break;
        case Weather::Cloudy: e.cloud = 0.62f; e.overcast = 0.55f; break;
        case Weather::Rain: e.cloud = 0.85f; e.overcast = 0.80f; e.rain = 0.75f; e.wet = 0.85f; e.wind = 2.5f; break;
        case Weather::Fog: e.cloud = 0.70f; e.overcast = 0.70f; e.wet = 0.25f; break;
        case Weather::Storm: e.cloud = 1.0f; e.overcast = 0.95f; e.rain = 1.0f; e.wet = 1.0f; e.storm = 1.0f; e.wind = 7.0f; break;
    }
    e.waveAmp = 0.9f + 1.4f * e.storm + 0.3f * e.rain;

    // time palettes -------------------------------------------------------------------------------
    float wDay = smoothstep(0.03f, 0.35f, e.sunElev);
    float wNight = 1.0f - smoothstep(-0.25f, -0.02f, e.sunElev);
    float wTw = saturate(1.0f - wDay - wNight);
    e.nightness = wNight;

    vec3 zenDay(0.10f, 0.26f, 0.62f), horDay(0.62f, 0.74f, 0.88f), glowDay(1.0f, 0.88f, 0.65f);
    vec3 zenTw(0.045f, 0.085f, 0.22f), horTw(0.62f, 0.30f, 0.17f), glowTw(1.0f, 0.40f, 0.12f);
    vec3 zenNt(0.004f, 0.008f, 0.024f), horNt(0.022f, 0.032f, 0.068f);
    e.zenith = zenDay * wDay + zenTw * wTw + zenNt * wNight;
    e.horizon = horDay * wDay + horTw * wTw + horNt * wNight;
    e.glow = (glowDay * 0.32f * wDay + glowTw * 1.5f * wTw) * (e.sunElev > -0.05f ? 1.0f : 0.0f);

    float lumZ = glm::dot(e.zenith, vec3(0.333f)), lumH = glm::dot(e.horizon, vec3(0.333f));
    e.zenith = mixv(e.zenith, vec3(lumZ * 0.85f), e.overcast * 0.75f);
    e.horizon = mixv(e.horizon, vec3(lumH * 0.95f), e.overcast * 0.75f);
    if (s.weather == Weather::Storm) {
        e.zenith *= 0.45f;
        e.horizon *= 0.6f;
    }

    // key light -----------------------------------------------------------------------------------
    vec3 keyDay = vec3(1.0f, 0.93f, 0.82f) * 3.4f;
    vec3 keyTw = vec3(1.0f, 0.55f, 0.26f) * 1.15f;
    vec3 keyNt = vec3(0.38f, 0.48f, 0.80f) * 0.30f;
    if (wNight > 0.5f) {
        e.keyDir = e.moonDir;
        e.keyColor = keyNt;
    } else {
        e.keyDir = e.sunDir;
        e.keyColor = keyDay * wDay + keyTw * wTw + keyNt * wNight;
    }
    e.keyColor *= 1.0f - 0.82f * e.overcast;

    // ambient -------------------------------------------------------------------------------------
    vec3 ambSkyDay(0.34f, 0.44f, 0.60f), ambGrDay(0.20f, 0.19f, 0.14f);
    vec3 ambSkyTw(0.20f, 0.20f, 0.32f), ambGrTw(0.10f, 0.07f, 0.05f);
    vec3 ambSkyNt(0.030f, 0.045f, 0.085f), ambGrNt(0.010f, 0.012f, 0.018f);
    e.ambSky = ambSkyDay * wDay + ambSkyTw * wTw + ambSkyNt * wNight;
    e.ambGround = ambGrDay * wDay + ambGrTw * wTw + ambGrNt * wNight;
    float lumA = glm::dot(e.ambSky, vec3(0.333f));
    e.ambSky = mixv(e.ambSky, vec3(lumA * 1.1f), e.overcast * 0.6f) * (1.0f + 0.25f * e.overcast);
    if (s.weather == Weather::Storm) {
        e.ambSky *= 0.7f;
        e.ambGround *= 0.7f;
    }

    // visibility / fog --------------------------------------------------------------------------
    e.visRange = effectiveVisibility(s);
    e.fogDensity = 3.912f / e.visRange;
    vec3 fog = e.horizon * 0.95f;
    float lumF = glm::dot(fog, vec3(0.333f));
    float foggy = (s.weather == Weather::Fog) ? 0.65f : 0.25f * (1.0f - smoothstep(1500.0f, 6000.0f, e.visRange));
    fog = mixv(fog, vec3(lumF * 1.05f), foggy);
    e.fogColor = fog;

    // exposure / misc ---------------------------------------------------------------------------
    e.exposure = 0.95f * wDay + 1.35f * wTw + 3.4f * wNight;
    e.stars = wNight * (1.0f - 0.9f * e.overcast);
    e.dayWarm = wDay * (1.0f - 0.6f * e.overcast) + wTw * 0.3f;
    return e;
}

}  // namespace aegis
