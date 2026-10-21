# FITWING - counter-drone training suite

Two independent apps (each has its own CMakeLists.txt, include/, src/, resources/):

| folder | what | tech |
|---|---|---|
| `aegis_command/` | **trainer / launcher**: operators, campaign, AI training, dashboard, after-action report (+PDF) | Qt6 Widgets, nlohmann json, Python/PyTorch recommender (C++ fallback) |
| `aegis_sim/` | **first-person simulator** (fixed post, night-shift atmosphere) | OpenGL 3.3 core, GLFW, GLEW, GLM, tinygltf, stb_image |


`requirements.txt` lists every `.glb` the simulator can use, with exact filenames, contents, sizes, colours, textures.

## Build

```
cmake -S aegis_sim     -B build/sim -DTINYGLTF_INCLUDE_DIR=<dir with tiny_gltf.h> -DSTB_INCLUDE_DIR=<dir with stb_image.h> [-DFITWING_DEPS_ROOT=<root with include/ lib/>]
cmake --build build/sim --config Release

cmake -S aegis_command -B build/cmd [-DCMAKE_PREFIX_PATH=<Qt6 prefix>] [-DFITWING_DEPS_ROOT=<root containing include/nlohmann/json.hpp>]
cmake --build build/cmd --config Release
```
`FITWING_GLEW_STATIC=ON` links GLEW statically. nlohmann json can be a CMake package, `<nlohmann/json.hpp>`, or a bare `json.hpp`.

## Deployment folder
```
FitWingCommand.exe   FitWingSim.exe   <all dlls>
shaders/           (from aegis_sim/shaders)
resources/         (from aegis_sim/resources - your .glb / .png files)
```
The trainer starts `FitWingSim` from its own folder (working directory = that folder). Qt resources (theme, Python recommender) are compiled into FitWingCommand.exe. Operator profiles are written to `AppData/` next to the exe (falls back to the user's app-data folder if not writable).

## The three JSON files
* `scene_definition.json` - trainer -> simulator (`--scene <file>`). Mission block `{id, mode, difficulty_level}` plus the chosen option of every parameter; written to `AppData/runtime/`.
* `player_performance.json` - simulator -> trainer (`--out <file>`). Mission id must match, otherwise the result is ignored. Written when the mission ends or is aborted.
* `player_stats.json` - owned by the trainer, one per operator (`AppData/players/<name>/`): a 0-100 mastery value (start 50) for every option value.
  Update rule per option present in the mission: `e = score - stat`; failing an easy mission costs more than failing a hard one, succeeding at a hard one gives more than succeeding at an easy one.
* Mission id = seconds since epoch. History / campaign progress live in memory only (no extra files).

## AI training
`aegis_command/resources/ai/recommender.py` (PyTorch) scores 400 valid candidate missions by weakness, expected score (ridge regression on this session's history) and novelty. If Python or torch is missing, an identical C++ implementation is used. The Settings page lets you switch it off.


## Simulator controls
LMB raise/lower current item - RMB next item - Q previous - mouse wheel zoom - SPACE act/tag/fire - R reload - SHIFT steady - 1/2/3 classify (single / swarm / multiple swarms) - ENTER start - ESC abort - F12 screenshot.
Items: rifle, missile launcher, binoculars (auto night vision at night), thermal imager, acoustic tablet, RF tablet.
Dev flags: `--scene --out --windowed --width --height --msaa --norender --bot --shot`.

## Notes
* The simulator has **no audio** (no audio library was requested/available); the acoustic tablet is visual.
* Trainer dev flags: `--selftest`, `--data <dir>`, `--screenshot <dir>` (use `QT_QPA_PLATFORM=offscreen`).
* "Training dummy" (Settings) lets the trainer run without the simulator for testing; such results are labelled.

