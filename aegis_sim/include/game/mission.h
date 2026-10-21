#pragma once
// Mission logic: drone swarm flight paths, the detect -> classify -> respond stage machine,
// weapons, sensors, decoys, scoring and the player_performance.json report.

#include <string>

#include "core/scene.h"
#include "game/world.h"
#include "gfx/environment.h"
#include "gfx/terrain.h"

namespace aegis {

constexpr float kBaseFovY = 62.0f;   // degrees, naked-eye view
float itemFovY(Item item, float zoom);  // radians; vertical FOV of the zoomed image

struct MissionResult {
    float detectionTime = 0.f, classificationTime = 0.f, responseTime = 0.f;
    int shotsFired = 0, missilesFired = 0;
    float decisionScore = 0.f, timeScore = 0.f, accuracy = 0.f, overallScore = 0.f;
    bool completed = false;
};

class Mission {
public:
    Mission(const SceneDef& scene, const Environment& env, const Terrain& terrain, uint32_t seed);

    void update(float dt, const Input& in);
    bool finished() const { return finished_; }
    const MissionResult& result() const { return result_; }
    bool writePerformance(const std::string& path) const;

    // queries used by HUD / renderer
    float detectWindow() const { return Td_; }
    float classifyWindow() const { return Tc_; }
    float respondWindow() const { return Tr_; }
    float impactTime() const { return tImpact_; }
    float stageRemaining() const;     // seconds left in the current stage window
    int aliveDrones() const;
    int totalDrones() const { return scene_.totalDrones(); }
    vec3 aimDir() const;              // player view direction including sway/recoil
    float aimYaw() const;
    float aimPitch() const;
    std::string clockText() const;
    bool itemUsable(Item i) const;
    std::string itemStatus(Item i) const;
    const SceneDef& scene() const { return scene_; }
    const Environment& env() const { return env_; }
    const Terrain& terrain() const { return terrain_; }
    int visibleBirdFalseTargets() const { return 0; }

    World w;

private:
    void startRunning();
    void updatePlayer(float dt, const Input& in);
    void updateStage(float dt, const Input& in);
    void updateDrones(float dt);
    void updateWeapons(float dt, const Input& in);
    void updateSensors(float dt);
    void updateDecoys(float dt);
    void updateEffects(float dt);
    void updateEnvironmentEvents(float dt);
    void finish(Phase outcome);
    void computeResult(bool completed);

    void action(const Input& in);
    void tagVisual(Item item);
    void tagSensor(Item item);
    void tagSwarm(int swarmId, const char* how);
    void falseAlarm(const char* what);
    bool visibleTo(Item item, const vec3& p, float objSize, float zoom) const;
    void fireRifle();
    void fireLauncher();
    void updateLock(float dt);
    void detonateMissile(Missile& m);
    void killDrone(Drone& d, bool byMissile);

    vec3 swarmCenter(const Swarm& s, float t, vec3* vel) const;
    void buildSwarms(Rng& rng);
    void buildBriefing();
    void say(const std::string& text, vec3 color = vec3(0.7f, 1.f, 0.7f), float life = 6.f, bool radio = false);

    // effects
    void puff(const vec3& p, const vec3& v, float size, float grow, const vec4& col, float life, float heat = 0.5f, float drag = 0.8f);
    void glow(const vec3& p, const vec3& v, float size, float grow, const vec4& col, float life, float heat = 1.0f);
    void spark(const vec3& p, const vec3& v, float size, const vec4& col, float life);
    void explosion(const vec3& p, float scale, bool flashy);

    SceneDef scene_;
    Environment env_;
    const Terrain& terrain_;
    Rng rng_;
    uint32_t seed_ = 1;

    float Td_ = 45.f, Tc_ = 26.f, Tr_ = 20.f, tImpact_ = 90.f;
    float quiet_ = 5.f;
    bool finished_ = false;
    MissionResult result_;
    bool resultComputed_ = false;

    // stage bookkeeping (threat time)
    float detectTime_ = 0.f, classTime_ = 0.f;
    float classDoneAt_ = -1.f;
    float firstShotAt_ = -1.f;
    float respTimeEnd_ = 0.f;
    bool detectedByPlayer_ = false;

    // counters
    int shots_ = 0, hits_ = 0, missilesFired_ = 0, missileEffective_ = 0, falseAlarms_ = 0, kills_ = 0;
    int missileKillsTotal_ = 0;

    // scheduled events
    float nextFlock_ = 6.f, nextNoise_ = 3.f, nextLightning_ = 8.f, nextWave_ = 0.f, nextAudio_ = 0.f;
    std::vector<float> blastSchedule_;
    struct NoiseEvt { float bearing, until, strength; };
    std::vector<NoiseEvt> noiseEvts_;
    float nextLed_ = 0.f;
    float thunderAt_ = -1.f;
    int nextDroneId_ = 0;
};

}  // namespace aegis
