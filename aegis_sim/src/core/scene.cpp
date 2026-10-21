#include "core/scene.h"

#include <algorithm>
#include <initializer_list>
#include <utility>

#include "core/json_lite.h"
#include "core/paths.h"

namespace aegis {

namespace {

template <typename E>
bool pick(const jl::Value& obj, const char* key, std::initializer_list<std::pair<const char*, E>> table, E& out,
          std::string& err, const char* path) {
    const jl::Value* v = obj.find(key);
    if (!v || !v->isString()) {
        err = std::string("scene_definition.json: missing string value '") + path + "'";
        return false;
    }
    for (const auto& kv : table) {
        if (v->str == kv.first) {
            out = kv.second;
            return true;
        }
    }
    err = std::string("scene_definition.json: invalid value '") + v->str + "' for '" + path + "'";
    return false;
}

bool pickInt(const jl::Value& obj, const char* key, std::initializer_list<int> allowed, int& out, std::string& err,
             const char* path) {
    const jl::Value* v = obj.find(key);
    if (!v || !v->isNumber()) {
        err = std::string("scene_definition.json: missing number '") + path + "'";
        return false;
    }
    int n = v->asInt();
    for (int a : allowed) {
        if (a == n) {
            out = n;
            return true;
        }
    }
    err = std::string("scene_definition.json: invalid number for '") + path + "'";
    return false;
}

// Template files list every allowed value as an array; accept those by taking the first entry.
void flattenArrays(jl::Value& v) {
    if (v.isArray() && !v.arr.empty()) {
        jl::Value first = v.arr.front();
        v = first;
    }
    if (v.isObject())
        for (auto& c : v.vals) flattenArrays(c);
}

}  // namespace

int SceneDef::resolvedSwarms() const {
    switch (threatType) {
        case ThreatType::Single: return 1;
        case ThreatType::Swarm: return 1;
        case ThreatType::MultipleSwarm: return std::max(2, swarmCount);
    }
    return 1;
}

int SceneDef::resolvedDronesPerSwarm() const {
    switch (threatType) {
        case ThreatType::Single: return 1;
        default: return std::max(2, dronesPerSwarm);
    }
}

bool loadScene(const std::string& path, SceneDef& s, std::string& err) {
    std::string text;
    if (!readTextFile(path, text)) {
        err = "cannot open scene file: " + path;
        return false;
    }
    jl::Value root;
    std::string perr;
    if (!jl::parse(text, root, perr)) {
        err = "scene_definition.json is not valid JSON: " + perr;
        return false;
    }
    flattenArrays(root);
    if (!root.isObject()) {
        err = "scene_definition.json: root must be an object";
        return false;
    }

    const jl::Value& mission = root.at("mission");
    if (!mission.isObject()) {
        err = "scene_definition.json: missing 'mission'";
        return false;
    }
    if (!mission.at("id").isNumber()) {
        err = "scene_definition.json: mission.id must be a number";
        return false;
    }
    s.missionId = mission.at("id").asInt();
    s.mode = mission.at("mode").asString("campaign");
    s.difficulty = std::max(1, std::min(5, mission.at("difficulty_level").asInt(1)));

    const jl::Value& env = root.at("environment");
    const jl::Value& sen = root.at("sensors");
    const jl::Value& inf = root.at("information");
    const jl::Value& dis = root.at("distractions");
    const jl::Value& thr = root.at("threat");
    const jl::Value& tp = root.at("time_pressure");
    for (const jl::Value* sec : {&env, &sen, &inf, &dis, &thr, &tp}) {
        if (!sec->isObject()) {
            err = "scene_definition.json: a required section is missing (environment, sensors, information, "
                  "distractions, threat, time_pressure)";
            return false;
        }
    }

    bool ok = true;
    ok = ok && pick<TimeOfDay>(env, "time", {{"day", TimeOfDay::Day}, {"twilight", TimeOfDay::Twilight}, {"night", TimeOfDay::Night}},
                               s.time, err, "environment.time");
    ok = ok && pick<Weather>(env, "weather",
                             {{"clear", Weather::Clear}, {"cloudy", Weather::Cloudy}, {"rain", Weather::Rain},
                              {"fog", Weather::Fog}, {"storm", Weather::Storm}},
                             s.weather, err, "environment.weather");
    ok = ok && pick<TerrainType>(env, "terrain",
                                 {{"rural", TerrainType::Rural}, {"urban", TerrainType::Urban},
                                  {"forest", TerrainType::Forest}, {"coastal", TerrainType::Coastal}},
                                 s.terrain, err, "environment.terrain");
    ok = ok && pick<Visibility>(env, "visibility",
                                {{"excellent", Visibility::Excellent}, {"good", Visibility::Good},
                                 {"moderate", Visibility::Moderate}, {"poor", Visibility::Poor},
                                 {"very_poor", Visibility::VeryPoor}},
                                s.visibility, err, "environment.visibility");

    const std::initializer_list<std::pair<const char*, SensorState>> sensorTable = {
        {"available", SensorState::Available}, {"degraded", SensorState::Degraded}, {"unavailable", SensorState::Unavailable}};
    ok = ok && pick<SensorState>(sen, "binoculars", sensorTable, s.binoculars, err, "sensors.binoculars");
    ok = ok && pick<AudioLevel>(sen, "audio", {{"high", AudioLevel::High}, {"medium", AudioLevel::Medium}, {"low", AudioLevel::Low}},
                                s.audio, err, "sensors.audio");
    ok = ok && pick<SensorState>(sen, "signal", sensorTable, s.signal, err, "sensors.signal");
    ok = ok && pick<SensorState>(sen, "thermal", sensorTable, s.thermal, err, "sensors.thermal");

    ok = ok && pick<Clue>(inf, "clue", {{"none", Clue::None}, {"partial", Clue::Partial}, {"detailed", Clue::Detailed}}, s.clue, err,
                          "information.clue");
    ok = ok && pick<Decoys>(dis, "decoys", {{"none", Decoys::None}, {"noise", Decoys::Noise}, {"blast", Decoys::Blast}}, s.decoys, err,
                            "distractions.decoys");

    ok = ok && pick<ThreatType>(thr, "type",
                                {{"single", ThreatType::Single}, {"swarm", ThreatType::Swarm},
                                 {"multiple_swarm", ThreatType::MultipleSwarm}},
                                s.threatType, err, "threat.type");
    ok = ok && pickInt(thr, "swarm_count", {1, 2, 3}, s.swarmCount, err, "threat.swarm_count");
    ok = ok && pickInt(thr, "drone_count_per_swarm", {1, 5, 10}, s.dronesPerSwarm, err, "threat.drone_count_per_swarm");
    ok = ok && pick<SwarmPosition>(thr, "position", {{"same", SwarmPosition::Same}, {"different", SwarmPosition::Different}},
                                   s.position, err, "threat.position");
    ok = ok && pick<Approach>(thr, "approach",
                              {{"stationary", Approach::Stationary}, {"crossing", Approach::Crossing},
                               {"converging", Approach::Converging}},
                              s.approach, err, "threat.approach");
    ok = ok && pick<Speed>(thr, "speed", {{"slow", Speed::Slow}, {"normal", Speed::Normal}, {"fast", Speed::Fast}}, s.speed, err,
                           "threat.speed");

    const std::initializer_list<std::pair<const char*, Window>> winTable = {
        {"long", Window::Long}, {"moderate", Window::Moderate}, {"short", Window::Short}, {"very_short", Window::VeryShort}};
    ok = ok && pick<Window>(tp, "detection_window", winTable, s.detectionWindow, err, "time_pressure.detection_window");
    ok = ok && pick<Window>(tp, "classification_window", winTable, s.classificationWindow, err,
                            "time_pressure.classification_window");
    ok = ok && pick<Window>(tp, "response_window", winTable, s.responseWindow, err, "time_pressure.response_window");
    return ok;
}

float detectionWindowSeconds(Window w) {
    switch (w) {
        case Window::Long: return 70.0f;
        case Window::Moderate: return 45.0f;
        case Window::Short: return 28.0f;
        case Window::VeryShort: return 16.0f;
    }
    return 45.0f;
}
float classificationWindowSeconds(Window w) {
    switch (w) {
        case Window::Long: return 40.0f;
        case Window::Moderate: return 26.0f;
        case Window::Short: return 16.0f;
        case Window::VeryShort: return 9.0f;
    }
    return 26.0f;
}
float responseWindowSeconds(Window w) {
    switch (w) {
        case Window::Long: return 30.0f;
        case Window::Moderate: return 20.0f;
        case Window::Short: return 12.0f;
        case Window::VeryShort: return 7.0f;
    }
    return 20.0f;
}
float speedMetersPerSecond(Speed s) {
    switch (s) {
        case Speed::Slow: return 9.0f;
        case Speed::Normal: return 20.0f;
        case Speed::Fast: return 38.0f;
    }
    return 20.0f;
}
float visibilityMeters(Visibility v) {
    switch (v) {
        case Visibility::Excellent: return 9000.0f;
        case Visibility::Good: return 3500.0f;
        case Visibility::Moderate: return 1300.0f;
        case Visibility::Poor: return 520.0f;
        case Visibility::VeryPoor: return 230.0f;
    }
    return 3000.0f;
}

const char* toString(TimeOfDay v) {
    switch (v) {
        case TimeOfDay::Day: return "day";
        case TimeOfDay::Twilight: return "twilight";
        case TimeOfDay::Night: return "night";
    }
    return "?";
}
const char* toString(Weather v) {
    switch (v) {
        case Weather::Clear: return "clear";
        case Weather::Cloudy: return "cloudy";
        case Weather::Rain: return "rain";
        case Weather::Fog: return "fog";
        case Weather::Storm: return "storm";
    }
    return "?";
}
const char* toString(TerrainType v) {
    switch (v) {
        case TerrainType::Rural: return "rural";
        case TerrainType::Urban: return "urban";
        case TerrainType::Forest: return "forest";
        case TerrainType::Coastal: return "coastal";
    }
    return "?";
}
const char* toString(Visibility v) {
    switch (v) {
        case Visibility::Excellent: return "excellent";
        case Visibility::Good: return "good";
        case Visibility::Moderate: return "moderate";
        case Visibility::Poor: return "poor";
        case Visibility::VeryPoor: return "very_poor";
    }
    return "?";
}
const char* toString(ThreatType v) {
    switch (v) {
        case ThreatType::Single: return "single";
        case ThreatType::Swarm: return "swarm";
        case ThreatType::MultipleSwarm: return "multiple_swarm";
    }
    return "?";
}
const char* toString(Approach v) {
    switch (v) {
        case Approach::Stationary: return "stationary";
        case Approach::Crossing: return "crossing";
        case Approach::Converging: return "converging";
    }
    return "?";
}

}  // namespace aegis
