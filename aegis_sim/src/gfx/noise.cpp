#include "gfx/noise.h"

#include <algorithm>
#include <cmath>

namespace aegis {
namespace noise {

static inline uint32_t ihash(int x, int y, int seed) {
    uint32_t h = static_cast<uint32_t>(x) * 374761393u + static_cast<uint32_t>(y) * 668265263u +
                 static_cast<uint32_t>(seed) * 2246822519u;
    h = (h ^ (h >> 13)) * 1274126177u;
    h ^= h >> 16;
    return h;
}

float hash2(int x, int y, int seed) { return (ihash(x, y, seed) >> 8) * (1.0f / 16777216.0f); }

static inline float fade(float t) { return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f); }
static inline float mix(float a, float b, float t) { return a + (b - a) * t; }

float value2(float x, float y, int seed) {
    int xi = static_cast<int>(std::floor(x));
    int yi = static_cast<int>(std::floor(y));
    float fx = x - xi, fy = y - yi;
    float a = hash2(xi, yi, seed), b = hash2(xi + 1, yi, seed);
    float c = hash2(xi, yi + 1, seed), d = hash2(xi + 1, yi + 1, seed);
    float u = fade(fx), v = fade(fy);
    return mix(mix(a, b, u), mix(c, d, u), v);
}

float fbm2(float x, float y, int octaves, float lacunarity, float gain, int seed) {
    float sum = 0.0f, amp = 0.5f, norm = 0.0f;
    for (int i = 0; i < octaves; ++i) {
        sum += amp * value2(x, y, seed + i * 131);
        norm += amp;
        x *= lacunarity;
        y *= lacunarity;
        amp *= gain;
    }
    return sum / norm;
}

float ridged2(float x, float y, int octaves, int seed) {
    float sum = 0.0f, amp = 0.5f, norm = 0.0f;
    for (int i = 0; i < octaves; ++i) {
        float n = value2(x, y, seed + i * 197);
        n = 1.0f - std::fabs(n * 2.0f - 1.0f);
        sum += amp * n * n;
        norm += amp;
        x *= 2.0f;
        y *= 2.0f;
        amp *= 0.5f;
    }
    return sum / norm;
}

static inline int wrapi(int v, int p) {
    v %= p;
    return v < 0 ? v + p : v;
}

float valueTile(float x, float y, int period, int seed) {
    float fx = x * period, fy = y * period;
    int xi = static_cast<int>(std::floor(fx));
    int yi = static_cast<int>(std::floor(fy));
    float tx = fx - xi, ty = fy - yi;
    int x0 = wrapi(xi, period), x1 = wrapi(xi + 1, period);
    int y0 = wrapi(yi, period), y1 = wrapi(yi + 1, period);
    float a = hash2(x0, y0, seed), b = hash2(x1, y0, seed);
    float c = hash2(x0, y1, seed), d = hash2(x1, y1, seed);
    float u = fade(tx), v = fade(ty);
    return mix(mix(a, b, u), mix(c, d, u), v);
}

float fbmTile(float x, float y, int basePeriod, int octaves, int seed) {
    float sum = 0.0f, amp = 0.5f, norm = 0.0f;
    int p = basePeriod;
    for (int i = 0; i < octaves; ++i) {
        sum += amp * valueTile(x, y, p, seed + i * 131);
        norm += amp;
        p *= 2;
        amp *= 0.5f;
    }
    return sum / norm;
}

float ridgedTile(float x, float y, int basePeriod, int octaves, int seed) {
    float sum = 0.0f, amp = 0.5f, norm = 0.0f;
    int p = basePeriod;
    for (int i = 0; i < octaves; ++i) {
        float n = valueTile(x, y, p, seed + i * 197);
        n = 1.0f - std::fabs(n * 2.0f - 1.0f);
        sum += amp * n * n;
        norm += amp;
        p *= 2;
        amp *= 0.5f;
    }
    return sum / norm;
}

float cellTile(float x, float y, int period, int seed, float* f2out) {
    float fx = x * period, fy = y * period;
    int xi = static_cast<int>(std::floor(fx));
    int yi = static_cast<int>(std::floor(fy));
    float best = 1e9f, second = 1e9f;
    for (int j = -1; j <= 1; ++j) {
        for (int i = -1; i <= 1; ++i) {
            int cx = xi + i, cy = yi + j;
            int wx = wrapi(cx, period), wy = wrapi(cy, period);
            float px = cx + hash2(wx, wy, seed);
            float py = cy + hash2(wx, wy, seed + 77);
            float dx = px - fx, dy = py - fy;
            float d = std::sqrt(dx * dx + dy * dy);
            if (d < best) {
                second = best;
                best = d;
            } else if (d < second) {
                second = d;
            }
        }
    }
    if (f2out) *f2out = second;
    return best;
}

}  // namespace noise
}  // namespace aegis
