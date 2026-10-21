#pragma once
// Dynamic game state shared by the mission logic, the renderer and the HUD.

#include <array>
#include <deque>
#include <string>
#include <vector>

#include "core/common.h"
#include "core/scene.h"

namespace aegis {

enum class Item { Binoculars = 0, Thermal, Rifle, Launcher, Acoustic, Rf, Count };
constexpr int kItemCount = static_cast<int>(Item::Count);
const char* itemName(Item i);

struct Input {
    float mdx = 0.f, mdy = 0.f;   // mouse delta (pixels)
    float scroll = 0.f;
    bool lmb = false, rmb = false;       // pressed this frame
    bool space = false;                  // action: tag / fire / launch
    bool reload = false;
    bool prevItem = false;
    bool key1 = false, key2 = false, key3 = false;
    bool enter = false, escape = false;
    bool steady = false;                 // held
    bool anyKey = false;
};

struct Player {
    float yaw = 0.f;     // radians, compass bearing (0 = north = -Z, +90 deg = east = +X)
    float pitch = 0.f;
    Item item = Item::Binoculars;
    bool inUse = false;
    float raise = 0.f;   // 0 stowed .. 1 fully raised
    float zoom[kItemCount] = {10.f, 5.f, 6.f, 3.f, 1.f, 1.f};
    float staticBurst = 0.f;
    float recoil = 0.f;
    float swayT = 0.f;
    vec2 sway{0.f};
    float breath = 1.f;  // steady-hold stamina
    // weapons
    int rifleMag = 10, rifleReserve = 60;
    int missiles = 3;
    float rifleCooldown = 0.f, rifleReload = 0.f, launcherReload = 0.f;
    int lockTarget = -1;
    float lockTimer = 0.f;
    bool locked = false;
    float itemSwitchT = 0.f;

    vec3 eye() const { return vec3(0.f, 11.65f, 0.f); }
    vec3 viewDir() const {
        return vec3(std::sin(yaw) * std::cos(pitch), std::sin(pitch), -std::cos(yaw) * std::cos(pitch));
    }
};

struct Drone {
    int id = 0;
    int swarm = 0;
    vec3 pos{0.f};
    vec3 vel{0.f};
    vec3 fwd{0.f, 0.f, -1.f};
    float hp = 1.f;
    enum State { Alive, Falling, Dead } state = Alive;
    float fallT = 0.f;
    float rotorPhase = 0.f;
    bool tagged = false;
    float seed = 0.f;
    vec3 offset{0.f};
    float bank = 0.f;
    float ledPhase = 0.f;
    float hitFlash = 0.f;
    bool alive() const { return state == Alive; }
};

struct Swarm {
    int id = 0;
    float bearing = 0.f;     // radians, direction the swarm comes from (compass)
    vec3 dir{0.f, 0.f, -1.f};// unit horizontal vector from the post towards the swarm origin
    vec3 tang{1.f, 0.f, 0.f};// unit horizontal tangent (crossing)
    float freq = 2.4f;       // GHz (RF)
    float startAlt = 80.f;
    float spread = 0.f;
    float distance = 1000.f; // transit start distance (converging) / hold distance (stationary)
    float closest = 300.f;   // crossing: closest approach distance
    float tEvent = 0.f;      // crossing: time of closest approach; stationary: time the dive starts
    float diveSpeed = 26.f;
    float slot = 0.f;        // small per-swarm time offset
    vec3 center{0.f};
    vec3 centerVel{0.f};
    bool detected = false;
    int alive = 0;
};

struct Missile {
    vec3 pos{0.f}, vel{0.f};
    int target = -1;
    float life = 0.f;
    bool alive = false;
    float trail = 0.f;
};

struct Bullet {
    vec3 pos{0.f}, vel{0.f};
    float life = 0.f;
    bool alive = false;
};

struct Tracer {
    vec3 a{0.f}, b{0.f};
    float age = 0.f;
};

struct Particle {
    vec3 pos{0.f}, vel{0.f};
    float size = 1.f, grow = 0.f;
    vec4 color{1.f};
    float life = 1.f, maxLife = 1.f;
    int type = 0;          // 0 smoke puff, 1 fire glow, 2 flare/spark, 3 debris dot
    float heat = 0.5f;
    float drag = 0.f;
    float gravity = 0.f;
    float rot = 0.f, rotVel = 0.f;
    float fadeIn = 0.f;
    bool additive = false;
    float minPixels = 0.f;
};

struct Bird {
    vec3 pos{0.f}, vel{0.f};
    float phase = 0.f;
    float life = 0.f;
};

struct Blast {
    vec3 pos{0.f};
    float age = 0.f;
    bool active = false;
    float smokeT = 0.f;
};

struct Contact {
    float bearing = 0.f;     // measured bearing (radians)
    float trueBearing = 0.f;
    float range = 0.f;
    float strength = 0.f;    // 0..1
    int source = -1;         // swarm id, -1 = decoy
    float freq = 0.f;        // RF only
};

struct Message {
    std::string text;
    float age = 0.f;
    float life = 6.f;
    vec3 color{0.7f, 1.0f, 0.7f};
    bool radio = false;
};

enum class Phase { Briefing, Running, Success, Failure, Aborted };
enum class Stage { Detect, Classify, Respond, Done };

struct World {
    Player player;
    std::vector<Swarm> swarms;
    std::vector<Drone> drones;
    std::vector<Missile> missiles;
    std::vector<Bullet> bullets;
    std::vector<Tracer> tracers;
    std::vector<Particle> particles;
    std::vector<Bird> birds;
    std::vector<Blast> blasts;
    std::vector<Contact> acoustic;
    std::vector<Contact> rf;
    std::deque<std::array<uint8_t, 128>> waterfall;
    std::deque<float> audioWave;
    std::deque<Message> messages;

    float time = 0.f;            // seconds since the window opened
    float missionTime = 0.f;     // seconds since the briefing was accepted
    float threatTime = 0.f;      // seconds since the threat appeared (t = 0 at spawn)
    float flash = 0.f;           // lightning / blast flash 0..1
    float shake = 0.f;           // camera shake amplitude (radians-ish)
    float flicker = 0.f;         // cab light flicker 0..1
    float damage = 0.f;          // red damage overlay 0..1
    float acousticJam = 0.f;
    float rfJam = 0.f;
    float alarm = 0.f;           // proximity alarm 0..1

    Phase phase = Phase::Briefing;
    Stage stage = Stage::Detect;
    float stageStart = 0.f;      // threatTime when the current stage started
    float endTimer = 0.f;        // time since the outcome was decided
    bool jumpscare = false;
    int classChoice = 0;         // 0 none, 1 single, 2 swarm, 3 multi
    bool classTimedOut = false;
    bool detectMissed = false;
    std::string briefing;
    std::string outcomeTitle, outcomeSub;
};

}  // namespace aegis
