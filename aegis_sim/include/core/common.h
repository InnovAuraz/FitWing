#pragma once
// Common includes, math helpers, logging and a small deterministic RNG.

#include <GL/glew.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/constants.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

using glm::mat3;
using glm::mat4;
using glm::vec2;
using glm::vec3;
using glm::vec4;

namespace aegis {

constexpr float kPi = 3.14159265358979323846f;
constexpr float kTwoPi = 6.28318530717958647692f;

inline float radians(float d) { return d * (kPi / 180.0f); }
inline float degrees(float r) { return r * (180.0f / kPi); }
inline float saturate(float x) { return std::min(1.0f, std::max(0.0f, x)); }
inline float lerp(float a, float b, float t) { return a + (b - a) * t; }
inline float smoothstep(float e0, float e1, float x) {
    float t = saturate((x - e0) / (e1 - e0));
    return t * t * (3.0f - 2.0f * t);
}
inline float wrapAngle(float a) {  // to [-pi, pi]
    while (a > kPi) a -= kTwoPi;
    while (a < -kPi) a += kTwoPi;
    return a;
}

// Logging -------------------------------------------------------------------
void logLine(const char* level, const char* fmt, ...);
#define LOG_INFO(...) ::aegis::logLine("INFO", __VA_ARGS__)
#define LOG_WARN(...) ::aegis::logLine("WARN", __VA_ARGS__)
#define LOG_ERROR(...) ::aegis::logLine("ERROR", __VA_ARGS__)

// PCG32 random generator -------------------------------------------------------
class Rng {
public:
    explicit Rng(uint64_t seed = 0x853c49e6748fea9bULL) { reseed(seed); }
    void reseed(uint64_t seed) {
        state_ = 0u;
        inc_ = (seed << 1u) | 1u;
        next();
        state_ += seed ^ 0x9e3779b97f4a7c15ULL;
        next();
    }
    uint32_t next() {
        uint64_t old = state_;
        state_ = old * 6364136223846793005ULL + inc_;
        uint32_t xorshifted = static_cast<uint32_t>(((old >> 18u) ^ old) >> 27u);
        uint32_t rot = static_cast<uint32_t>(old >> 59u);
        return (xorshifted >> rot) | (xorshifted << ((~rot + 1u) & 31u));
    }
    float uniform() { return (next() >> 8) * (1.0f / 16777216.0f); }  // [0,1)
    float uniform(float a, float b) { return a + (b - a) * uniform(); }
    int range(int lo, int hi) {  // inclusive
        if (hi <= lo) return lo;
        return lo + static_cast<int>(next() % static_cast<uint32_t>(hi - lo + 1));
    }
    bool chance(float p) { return uniform() < p; }
    float gauss() {  // approx N(0,1)
        float s = 0.0f;
        for (int i = 0; i < 6; ++i) s += uniform();
        return (s - 3.0f) * 1.41421356f;
    }
    vec3 unitSphere() {
        float z = uniform(-1.0f, 1.0f);
        float a = uniform(0.0f, kTwoPi);
        float r = std::sqrt(std::max(0.0f, 1.0f - z * z));
        return vec3(r * std::cos(a), z, r * std::sin(a));
    }

private:
    uint64_t state_ = 0, inc_ = 1;
};

// Cheap integer hash -> [0,1)
inline float hash01(uint32_t x) {
    x ^= x >> 16; x *= 0x7feb352dU; x ^= x >> 15; x *= 0x846ca68bU; x ^= x >> 16;
    return (x >> 8) * (1.0f / 16777216.0f);
}

}  // namespace aegis
