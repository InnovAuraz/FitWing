#pragma once
// Lighting / atmosphere derived from the scene definition (time, weather, visibility).

#include "core/scene.h"
#include "core/common.h"

namespace aegis {

constexpr float kTowerHeight = 10.0f;   // cab floor above the terrain at the post
constexpr float kEyeHeight = 11.65f;    // eye above the terrain at the post

struct Environment {
    vec3 sunDir{0, 1, 0};   // towards the sun
    vec3 moonDir{0, 1, 0};  // towards the moon
    float sunElev = 1.0f;   // sin(elevation) of the sun
    vec3 keyDir{0, 1, 0};   // direction towards the key light (sun by day, moon by night)
    vec3 keyColor{1.f};
    vec3 zenith{0.f}, horizon{0.f}, glow{0.f};
    vec3 ambSky{0.f}, ambGround{0.f};
    vec3 fogColor{0.f};
    float fogDensity = 0.0003f;
    float visRange = 10000.f;
    float exposure = 1.0f;
    float cloud = 0.2f;      // coverage 0..1
    float overcast = 0.0f;   // 0..1
    float storm = 0.0f;      // 0..1
    float rain = 0.0f;       // 0..1
    float wet = 0.0f;        // 0..1
    float stars = 0.0f;      // 0..1
    float dayWarm = 0.0f;    // how much the ground is sun-warmed (thermal)
    float wind = 0.0f;       // m/s lateral rain drift
    float waveAmp = 1.0f;
    float nightness = 0.0f;  // 1 at night
    float cabFlickerBase = 0.0f;
    float sunAzimuth = 0.0f;
};

Environment makeEnvironment(const SceneDef& s, uint32_t seed);

// Effective visibility distance in metres (min of the visibility class and the weather cap).
float effectiveVisibility(const SceneDef& s);

}  // namespace aegis
