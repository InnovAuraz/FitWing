#pragma once
// Procedural guard-tower cab (the player's post): floor, waist-high walls, posts, roof, desk with
// monitors, ceiling lamp, catwalk with railing and the lattice tower legs below.
// Part names with special meaning for the renderer:
//   lamp_bulb   -> emissive scaled by the lamp intensity
//   alarm_lamp  -> emissive scaled by the proximity alarm
//   monitor_*   -> screens, always faintly lit

#include <memory>

#include "gfx/model.h"

namespace aegis {

std::unique_ptr<Model> buildCab();
constexpr float kCabFloorY = 10.0f;
// Lamp position (world) used as a point light.
inline vec3 cabLampPos() { return vec3(0.35f, 12.45f, 0.25f); }

}  // namespace aegis
