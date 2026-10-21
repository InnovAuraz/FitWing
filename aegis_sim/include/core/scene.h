#pragma once
// Scene definition (mission description) exactly as produced by the Qt trainer.
// The simulator never decides *what* the mission is; it only executes this.

#include <string>

namespace aegis {

enum class TimeOfDay { Day, Twilight, Night };
enum class Weather { Clear, Cloudy, Rain, Fog, Storm };
enum class TerrainType { Rural, Urban, Forest, Coastal };
enum class Visibility { Excellent, Good, Moderate, Poor, VeryPoor };
enum class SensorState { Available, Degraded, Unavailable };
enum class AudioLevel { High, Medium, Low };
enum class Clue { None, Partial, Detailed };
enum class Decoys { None, Noise, Blast };
enum class ThreatType { Single, Swarm, MultipleSwarm };
enum class SwarmPosition { Same, Different };
enum class Approach { Stationary, Crossing, Converging };
enum class Speed { Slow, Normal, Fast };
enum class Window { Long, Moderate, Short, VeryShort };

struct SceneDef {
    // mission block (echoed verbatim into player_performance.json)
    int missionId = 0;
    std::string mode = "campaign";
    int difficulty = 1;

    TimeOfDay time = TimeOfDay::Day;
    Weather weather = Weather::Clear;
    TerrainType terrain = TerrainType::Rural;
    Visibility visibility = Visibility::Excellent;

    SensorState binoculars = SensorState::Available;
    AudioLevel audio = AudioLevel::Medium;
    SensorState signal = SensorState::Available;
    SensorState thermal = SensorState::Available;

    Clue clue = Clue::None;
    Decoys decoys = Decoys::None;

    ThreatType threatType = ThreatType::Single;
    int swarmCount = 1;
    int dronesPerSwarm = 1;
    SwarmPosition position = SwarmPosition::Same;
    Approach approach = Approach::Converging;
    Speed speed = Speed::Normal;

    Window detectionWindow = Window::Moderate;
    Window classificationWindow = Window::Moderate;
    Window responseWindow = Window::Moderate;

    // Resolved threat layout (the vocabulary allows combinations that do not make
    // sense, e.g. type=single with swarm_count=3; the simulator normalises them).
    int resolvedSwarms() const;
    int resolvedDronesPerSwarm() const;
    int totalDrones() const { return resolvedSwarms() * resolvedDronesPerSwarm(); }
};

// Parses and validates scene_definition.json.  On failure returns false and fills `err`.
bool loadScene(const std::string& path, SceneDef& out, std::string& err);

// Seconds represented by the qualitative time-pressure labels.
float detectionWindowSeconds(Window w);
float classificationWindowSeconds(Window w);
float responseWindowSeconds(Window w);
float speedMetersPerSecond(Speed s);
float visibilityMeters(Visibility v);

const char* toString(TimeOfDay v);
const char* toString(Weather v);
const char* toString(TerrainType v);
const char* toString(Visibility v);
const char* toString(ThreatType v);
const char* toString(Approach v);

}  // namespace aegis
