#pragma once
// CPU noise used for terrain, procedural textures and prop placement.

#include <cstdint>

namespace aegis {
namespace noise {

float hash2(int x, int y, int seed);        // [0,1)
float value2(float x, float y, int seed);   // smooth value noise, [0,1)
float fbm2(float x, float y, int octaves, float lacunarity, float gain, int seed);  // ~[0,1)
float ridged2(float x, float y, int octaves, int seed);                            // ~[0,1)

// Tileable versions: x and y are in [0,1) texture space; `period` lattice cells wrap seamlessly.
float valueTile(float x, float y, int period, int seed);
float fbmTile(float x, float y, int basePeriod, int octaves, int seed);
float ridgedTile(float x, float y, int basePeriod, int octaves, int seed);
// Worley noise (distance to nearest feature point) with wrapping; returns F1 in cell units (~0..1).
float cellTile(float x, float y, int period, int seed, float* f2 = nullptr);

}  // namespace noise
}  // namespace aegis
