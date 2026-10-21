#include "game/mission.h"

#include <cstdio>

#include "core/json_lite.h"
#include "core/paths.h"

namespace aegis {

namespace {
constexpr float kCabY = 11.65f;

float bearingOf(const vec3& v) {
    float b = std::atan2(v.x, -v.z);
    return b < 0.f ? b + kTwoPi : b;
}
vec3 dirOfBearing(float b) { return vec3(std::sin(b), 0.f, -std::cos(b)); }

const char* windName(float bearing) {
    static const char* n[8] = {"NORTH", "NORTH-EAST", "EAST", "SOUTH-EAST", "SOUTH", "SOUTH-WEST", "WEST", "NORTH-WEST"};
    int idx = static_cast<int>(std::floor(bearing / (kPi / 4.0f) + 0.5f)) & 7;
    return n[idx];
}

bool segSphere(const vec3& a, const vec3& b, const vec3& c, float r) {
    vec3 d = b - a;
    float l2 = glm::dot(d, d);
    float t = l2 > 1e-9f ? glm::clamp(glm::dot(c - a, d) / l2, 0.f, 1.f) : 0.f;
    vec3 p = a + d * t;
    return glm::dot(p - c, p - c) <= r * r;
}

float stageScore(float ratio) {
    if (ratio <= 0.25f) return 1.0f;
    return std::max(0.0f, 1.0f - (ratio - 0.25f) / 0.75f);
}

float smoothNoise(float t, float seed) {
    float x = t + seed * 17.0f;
    int i = static_cast<int>(std::floor(x));
    float f = x - i;
    f = f * f * (3.f - 2.f * f);
    return hash01(static_cast<uint32_t>(i) * 2654435761u + static_cast<uint32_t>(seed * 1000.f)) * (1.f - f) * 2.f - (1.f - f) +
           (hash01(static_cast<uint32_t>(i + 1) * 2654435761u + static_cast<uint32_t>(seed * 1000.f)) * 2.f - 1.f) * f;
}
}  // namespace

float itemFovY(Item item, float zoom) {
    (void)item;
    float base = radians(kBaseFovY);
    return 2.0f * std::atan(std::tan(base * 0.5f) / std::max(1.0f, zoom));
}

const char* itemName(Item i) {
    switch (i) {
        case Item::Binoculars: return "BINOCULARS";
        case Item::Thermal: return "THERMAL IMAGER";
        case Item::Rifle: return "RIFLE";
        case Item::Launcher: return "MISSILE LAUNCHER";
        case Item::Acoustic: return "ACOUSTIC MONITOR";
        case Item::Rf: return "RF SCANNER";
        default: return "?";
    }
}

// ---------------------------------------------------------------------------------------------
Mission::Mission(const SceneDef& scene, const Environment& env, const Terrain& terrain, uint32_t seed)
    : scene_(scene), env_(env), terrain_(terrain), rng_(seed * 977u + 5u), seed_(seed) {
    Td_ = detectionWindowSeconds(scene.detectionWindow);
    Tc_ = classificationWindowSeconds(scene.classificationWindow);
    Tr_ = responseWindowSeconds(scene.responseWindow);
    w.player.yaw = rng_.uniform(0.f, kTwoPi);
    w.player.pitch = 0.05f;
    buildSwarms(rng_);
    buildBriefing();
    w.phase = Phase::Briefing;
    for (const auto& sw : w.swarms) LOG_INFO("swarm %d bearing %.1f deg", sw.id, degrees(sw.bearing));
    nextFlock_ = rng_.uniform(5.f, 10.f);
    nextNoise_ = rng_.uniform(2.f, 5.f);
    nextLightning_ = rng_.uniform(3.f, 9.f);
    for (int i = 0; i < 64; ++i) w.audioWave.push_back(0.f);
    LOG_INFO("mission %d: swarms=%d drones/swarm=%d tImpact=%.1fs windows %.0f/%.0f/%.0f", scene_.missionId,
             scene_.resolvedSwarms(), scene_.resolvedDronesPerSwarm(), tImpact_, Td_, Tc_, Tr_);
}

void Mission::say(const std::string& text, vec3 color, float life, bool radio) {
    Message m;
    m.text = text;
    m.color = color;
    m.life = life;
    m.radio = radio;
    w.messages.push_back(m);
    while (w.messages.size() > 5) w.messages.pop_front();
}

void Mission::buildSwarms(Rng& rng) {
    int n = scene_.resolvedSwarms();
    int per = scene_.resolvedDronesPerSwarm();
    float vT = speedMetersPerSecond(scene_.speed);
    float Ttot = Td_ + Tc_ + Tr_;
    float b0 = rng.uniform(0.f, kTwoPi);
    w.swarms.clear();
    w.drones.clear();
    tImpact_ = 0.f;
    nextDroneId_ = 0;
    for (int k = 0; k < n; ++k) {
        Swarm s;
        s.id = k;
        float b;
        if (n == 1) b = b0;
        else if (scene_.position == SwarmPosition::Same) b = b0 + (k - (n - 1) * 0.5f) * radians(rng.uniform(14.f, 22.f));
        else b = b0 + k * kTwoPi / n + radians(rng.uniform(-20.f, 20.f));
        b = std::fmod(b + 4.f * kTwoPi, kTwoPi);
        s.bearing = b;
        s.dir = dirOfBearing(b);
        s.tang = vec3(std::cos(b), 0.f, std::sin(b)) * (rng.chance(0.5f) ? 1.f : -1.f);
        s.freq = 2.412f + 0.028f * k + rng.uniform(0.f, 0.012f) + (k % 2) * 0.03f;
        s.spread = per == 1 ? 0.f : (per <= 5 ? 6.5f : 11.f);
        s.diveSpeed = std::max(vT, 26.f);
        float impact = 0.f;
        switch (scene_.approach) {
            case Approach::Converging: {
                s.startAlt = rng.uniform(60.f, 105.f);
                s.distance = glm::clamp(vT * Ttot, 450.f, 3200.f);
                s.tEvent = s.distance / vT;
                impact = s.tEvent;
                break;
            }
            case Approach::Crossing: {
                s.closest = rng.uniform(260.f, 360.f);
                s.startAlt = rng.uniform(45.f, 85.f);
                float Ld = std::sqrt(s.closest * s.closest + (s.startAlt - kCabY) * (s.startAlt - kCabY));
                float tc = Ttot - Ld / s.diveSpeed;
                float Lt = glm::clamp(vT * tc, 450.f, 3200.f);
                s.distance = Lt;
                s.tEvent = Lt / vT;
                impact = s.tEvent + Ld / s.diveSpeed;
                break;
            }
            case Approach::Stationary: {
                s.distance = rng.uniform(380.f, 520.f);
                s.startAlt = rng.uniform(40.f, 75.f);
                float Ld = std::sqrt(s.distance * s.distance + (s.startAlt - kCabY) * (s.startAlt - kCabY));
                s.tEvent = std::max(6.f, Ttot - Ld / s.diveSpeed);
                impact = s.tEvent + Ld / s.diveSpeed;
                break;
            }
        }
        tImpact_ = std::max(tImpact_, impact);
        s.alive = per;
        for (int i = 0; i < per; ++i) {
            Drone d;
            d.id = nextDroneId_++;
            d.swarm = k;
            d.seed = rng.uniform(0.f, 100.f);
            d.ledPhase = rng.uniform(0.f, 1.f);
            d.rotorPhase = rng.uniform(0.f, kTwoPi);
            if (per > 1) {
                vec3 o(rng.gauss() * s.spread * 0.5f, rng.gauss() * s.spread * 0.2f, rng.gauss() * s.spread * 0.5f);
                float l = glm::length(o);
                if (l > s.spread) o *= s.spread / l;
                d.offset = o;
            }
            w.drones.push_back(d);
        }
        w.swarms.push_back(s);
    }
    for (auto& s : w.swarms) {
        s.center = swarmCenter(s, 0.f, &s.centerVel);
    }
    for (auto& d : w.drones) {
        d.pos = w.swarms[static_cast<size_t>(d.swarm)].center + d.offset;
        d.fwd = glm::normalize(-w.swarms[static_cast<size_t>(d.swarm)].dir);
    }
}

vec3 Mission::swarmCenter(const Swarm& s, float t, vec3* vel) const {
    auto pos = [&](float tt) -> vec3 {
        vec3 P(0.f, kCabY, 0.f);
        float vT = speedMetersPerSecond(scene_.speed);
        tt = std::max(tt, 0.f);
        switch (scene_.approach) {
            case Approach::Converging: {
                float gd = std::max(0.f, s.distance - vT * tt);
                float alt = kCabY + (s.startAlt - kCabY) * std::sqrt(gd / s.distance);
                float x = s.dir.x * gd, z = s.dir.z * gd;
                float weave = std::sin(tt * 0.35f + s.id) * 14.f * smoothstep(0.f, 400.f, gd);
                x += s.tang.x * weave;
                z += s.tang.z * weave;
                float y = std::max(alt, terrain_.height(x, z) + 22.f * smoothstep(0.f, 200.f, gd) + 3.f);
                return vec3(x, y, z);
            }
            case Approach::Crossing: {
                vec3 C = s.dir * s.closest + vec3(0.f, s.startAlt, 0.f);
                if (tt <= s.tEvent) {
                    vec3 p = C + s.tang * (vT * (tt - s.tEvent));
                    p.y += std::sin(tt * 0.4f + s.id) * 3.f;
                    p.y = std::max(p.y, terrain_.height(p.x, p.z) + 25.f);
                    return p;
                }
                float Ld = glm::length(P - C);
                float f = glm::clamp((tt - s.tEvent) * s.diveSpeed / Ld, 0.f, 1.f);
                return C + (P - C) * f;
            }
            case Approach::Stationary: {
                vec3 H = s.dir * s.distance + vec3(0.f, s.startAlt, 0.f);
                if (tt <= s.tEvent) {
                    H += vec3(std::sin(tt * 0.31f + s.id) * 3.f, std::sin(tt * 0.47f) * 1.5f, std::cos(tt * 0.27f + s.id) * 3.f);
                    return H;
                }
                float Ld = glm::length(P - H);
                float f = glm::clamp((tt - s.tEvent) * s.diveSpeed / Ld, 0.f, 1.f);
                return H + (P - H) * f;
            }
        }
        return P;
    };
    vec3 p = pos(t);
    if (vel) *vel = (pos(t + 0.05f) - p) / 0.05f;
    return p;
}

void Mission::buildBriefing() {
    char buf[512];
    std::string b;
    std::snprintf(buf, sizeof(buf), "MISSION %d\n", scene_.missionId);
    b += buf;
    b += clockText() + "\n";
    std::snprintf(buf, sizeof(buf), "TERRAIN: %s   WEATHER: %s   VISIBILITY: %s\n", toString(scene_.terrain), toString(scene_.weather),
                  toString(scene_.visibility));
    b += buf;
    for (char& c : b) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    auto stateName = [](SensorState s) { return s == SensorState::Available ? "OPERATIONAL" : (s == SensorState::Degraded ? "DEGRADED" : "OFFLINE"); };
    auto audName = [](AudioLevel a) { return a == AudioLevel::High ? "HIGH GAIN" : (a == AudioLevel::Medium ? "MEDIUM GAIN" : "LOW GAIN"); };
    std::snprintf(buf, sizeof(buf), "BINOCULARS: %s\nTHERMAL: %s\nACOUSTIC: %s\nRF SCANNER: %s\n", stateName(scene_.binoculars),
                  stateName(scene_.thermal), audName(scene_.audio), stateName(scene_.signal));
    b += buf;
    b += "\nINTEL:\n";
    switch (scene_.clue) {
        case Clue::None: b += "NO INTEL AVAILABLE. STAY ALERT."; break;
        case Clue::Partial: {
            std::snprintf(buf, sizeof(buf), "HOSTILE UAS ACTIVITY REPORTED TO THE %s. STRENGTH UNKNOWN.", windName(w.swarms.empty() ? 0.f : w.swarms[0].bearing));
            b += buf;
            break;
        }
        case Clue::Detailed: {
            std::string bearings;
            for (size_t i = 0; i < w.swarms.size(); ++i) {
                std::snprintf(buf, sizeof(buf), "%s%03d", i ? ", " : "", static_cast<int>(degrees(w.swarms[i].bearing) + 0.5f) % 360);
                bearings += buf;
            }
            const char* tt = scene_.threatType == ThreatType::Single ? "SINGLE" : (scene_.threatType == ThreatType::Swarm ? "SWARM" : "MULTIPLE SWARM");
            std::snprintf(buf, sizeof(buf), "%d HOSTILE UAS (%s) INBOUND, BEARING %s DEG.\nAPPROACH: %s. SPEED: %s.", totalDrones(), tt,
                          bearings.c_str(), toString(scene_.approach), scene_.speed == Speed::Slow ? "SLOW" : (scene_.speed == Speed::Normal ? "NORMAL" : "FAST"));
            std::string s2 = buf;
            for (char& c : s2) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
            b += s2;
            break;
        }
    }
    w.briefing = b;
}

std::string Mission::clockText() const {
    int base = 0;
    switch (scene_.time) {
        case TimeOfDay::Night: base = 0; break;
        case TimeOfDay::Twilight: base = 19 * 60 + 10; break;
        case TimeOfDay::Day: base = 13 * 60; break;
    }
    float prog = tImpact_ > 0.f ? glm::clamp(std::max(0.f, w.threatTime) / tImpact_, 0.f, 1.f) : 0.f;
    int mins = base + static_cast<int>(prog * (scene_.time == TimeOfDay::Night ? 360.f : 40.f));
    int h = (mins / 60) % 24, m = mins % 60;
    int h12 = h % 12 == 0 ? 12 : h % 12;
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%02d:%02d %s", h12, m, h < 12 ? "AM" : "PM");
    return buf;
}

bool Mission::itemUsable(Item i) const {
    switch (i) {
        case Item::Binoculars: return scene_.binoculars != SensorState::Unavailable;
        case Item::Thermal: return scene_.thermal != SensorState::Unavailable;
        case Item::Rf: return scene_.signal != SensorState::Unavailable;
        default: return true;
    }
}

std::string Mission::itemStatus(Item i) const {
    auto st = [](SensorState s) { return s == SensorState::Available ? "OPERATIONAL" : (s == SensorState::Degraded ? "DEGRADED" : "OFFLINE"); };
    switch (i) {
        case Item::Binoculars: return st(scene_.binoculars);
        case Item::Thermal: return st(scene_.thermal);
        case Item::Rf: return st(scene_.signal);
        case Item::Acoustic: return scene_.audio == AudioLevel::High ? "HIGH GAIN" : (scene_.audio == AudioLevel::Medium ? "MEDIUM GAIN" : "LOW GAIN");
        default: return "READY";
    }
}

int Mission::aliveDrones() const {
    int n = 0;
    for (const auto& d : w.drones)
        if (d.alive()) ++n;
    return n;
}

float Mission::stageRemaining() const {
    switch (w.stage) {
        case Stage::Detect: return std::max(0.f, Td_ - w.threatTime);
        case Stage::Classify: return std::max(0.f, Tc_ - (w.threatTime - w.stageStart));
        case Stage::Respond: return std::max(0.f, tImpact_ - w.threatTime);
        default: return 0.f;
    }
}

float Mission::aimYaw() const { return w.player.yaw + w.player.sway.x; }
float Mission::aimPitch() const { return w.player.pitch + w.player.sway.y + w.player.recoil; }
vec3 Mission::aimDir() const {
    float y = aimYaw(), p = aimPitch();
    return vec3(std::sin(y) * std::cos(p), std::sin(p), -std::cos(y) * std::cos(p));
}

// ---------------------------------------------------------------------------------------------
// Update
// ---------------------------------------------------------------------------------------------
void Mission::startRunning() {
    w.phase = Phase::Running;
    w.missionTime = 0.f;
    say("HQ: SHIFT STARTS. KEEP YOUR EYES OPEN.", vec3(0.6f, 1.f, 0.6f), 7.f, true);
    if (scene_.clue != Clue::None) say("HQ: CHECK THE INTEL BRIEF. REPORT CONTACTS.", vec3(0.6f, 1.f, 0.6f), 7.f, true);
    if (scene_.decoys == Decoys::Blast) {
        blastSchedule_.push_back(rng_.uniform(3.f, Td_ * 0.5f));
        blastSchedule_.push_back(rng_.uniform(Td_ * 0.55f, Td_ + Tc_ * 0.5f));
    }
}

void Mission::update(float dt, const Input& in) {
    dt = std::min(dt, 0.1f);
    w.time += dt;
    for (auto& m : w.messages) m.age += dt;
    while (!w.messages.empty() && w.messages.front().age > w.messages.front().life) w.messages.pop_front();

    if (w.phase == Phase::Briefing) {
        updatePlayer(dt, Input());
        if (in.enter || in.lmb || in.space) startRunning();
        if (in.escape) {
            computeResult(false);
            w.phase = Phase::Aborted;
            finished_ = true;
        }
        return;
    }
    if (in.escape && w.phase == Phase::Running) {
        finish(Phase::Aborted);
        return;
    }

    w.missionTime += dt;
    w.threatTime = w.missionTime - quiet_;
    if (w.phase == Phase::Running) {
        updatePlayer(dt, in);
        updateStage(dt, in);
        updateWeapons(dt, in);
    } else {
        updatePlayer(dt, Input());
        w.endTimer += dt;
        if ((in.enter || in.lmb) && w.endTimer > 2.0f) finished_ = true;
        if (w.endTimer > 16.f) finished_ = true;
    }
    updateDrones(dt);
    updateSensors(dt);
    updateDecoys(dt);
    updateEnvironmentEvents(dt);
    updateEffects(dt);
}

void Mission::updatePlayer(float dt, const Input& in) {
    Player& p = w.player;
    // item switching: lower, swap, raise
    if (w.phase == Phase::Running) {
        if (in.lmb) p.inUse = !p.inUse;
        if (in.rmb || in.prevItem) {
            int idx = static_cast<int>(p.item);
            idx = in.prevItem ? (idx + kItemCount - 1) % kItemCount : (idx + 1) % kItemCount;
            p.itemSwitchT = static_cast<float>(idx) + 1.0f;  // pending item (index + 1), 0 = none
            p.staticBurst = 0.35f;
        }
    }
    bool switching = p.itemSwitchT > 0.5f;
    float target = (p.inUse && !switching) ? 1.f : 0.f;
    float rate = target > p.raise ? 4.2f : 6.0f;
    p.raise = glm::clamp(p.raise + glm::clamp(target - p.raise, -rate * dt, rate * dt), 0.f, 1.f);
    if (switching && p.raise < 0.02f) {
        p.item = static_cast<Item>(static_cast<int>(p.itemSwitchT + 0.5f) - 1);
        p.itemSwitchT = 0.f;
        p.lockTimer = 0.f;
        p.lockTarget = -1;
        p.locked = false;
    }
    p.staticBurst = std::max(0.f, p.staticBurst - dt);

    // zoom
    int it = static_cast<int>(p.item);
    if (in.scroll != 0.f && p.raise > 0.5f) {
        float lo = 1.f, hi = 1.f;
        if (p.item == Item::Binoculars) { lo = 6.f; hi = 16.f; }
        if (p.item == Item::Thermal) { lo = 3.f; hi = 10.f; }
        if (p.item == Item::Rifle) { lo = 4.f; hi = 12.f; }
        if (hi > lo) p.zoom[it] = glm::clamp(p.zoom[it] * std::pow(1.12f, in.scroll), lo, hi);
    }
    // look
    float zoomEff = 1.f + (p.zoom[it] - 1.f) * p.raise;
    bool optical = p.item == Item::Binoculars || p.item == Item::Thermal || p.item == Item::Rifle || p.item == Item::Launcher;
    if (!optical) zoomEff = 1.f;
    float sens = 0.0021f / std::pow(zoomEff, 0.85f);
    p.yaw += in.mdx * sens;
    p.pitch = glm::clamp(p.pitch - in.mdy * sens, -0.62f, 1.25f);
    p.yaw = std::fmod(p.yaw + 4.f * kTwoPi, kTwoPi);

    // sway / breathing
    p.swayT += dt;
    bool steady = in.steady && p.breath > 0.02f;
    p.breath = glm::clamp(p.breath + (steady ? -dt / 4.5f : dt * 0.35f), 0.f, 1.f);
    float amp = optical && p.raise > 0.3f ? 0.0005f * std::pow(p.zoom[it], 0.75f) * (steady ? 0.18f : 1.f) * p.raise : 0.f;
    float t = p.swayT;
    p.sway = vec2(std::sin(t * 0.9f) + 0.6f * std::sin(t * 2.3f + 1.f), std::cos(t * 1.1f) + 0.5f * std::sin(t * 1.9f + 2.f)) * amp;
    p.recoil = std::max(0.f, p.recoil - dt * 0.12f - p.recoil * dt * 5.f);
    w.shake = std::max(0.f, w.shake - dt * 1.4f);
}

void Mission::updateStage(float dt, const Input& in) {
    (void)dt;
    float t = w.threatTime;
    if (t < 0.f) return;
    switch (w.stage) {
        case Stage::Detect:
            if (t >= Td_) {
                w.detectMissed = true;
                detectTime_ = Td_;
                for (auto& s : w.swarms) {
                    s.detected = true;
                }
                for (auto& d : w.drones) d.tagged = true;
                w.stage = Stage::Classify;
                w.stageStart = t;
                int nearest = 0;
                float bd = 1e9f;
                for (auto& s : w.swarms)
                    if (s.alive > 0 && glm::length(s.center) < bd) { bd = glm::length(s.center); nearest = s.id; }
                char buf[96];
                std::snprintf(buf, sizeof(buf), "RADAR ALERT: CONTACT BEARING %03d. CLASSIFY (1/2/3)",
                              static_cast<int>(degrees(w.swarms[static_cast<size_t>(nearest)].bearing) + 0.5f) % 360);
                say(buf, vec3(1.f, 0.55f, 0.2f), 8.f, true);
            }
            break;
        case Stage::Classify: {
            int choice = in.key1 ? 1 : (in.key2 ? 2 : (in.key3 ? 3 : 0));
            if (choice) {
                w.classChoice = choice;
                classTime_ = t - w.stageStart;
                classDoneAt_ = t;
                w.stage = Stage::Respond;
                w.stageStart = t;
                static const char* names[4] = {"", "SINGLE", "SWARM", "MULTI-SWARM"};
                say(std::string("CLASSIFICATION LOGGED: ") + names[choice] + ". ENGAGE.", vec3(0.6f, 1.f, 0.6f), 5.f);
            } else if (t - w.stageStart >= Tc_) {
                w.classTimedOut = true;
                classTime_ = Tc_;
                classDoneAt_ = t;
                w.stage = Stage::Respond;
                w.stageStart = t;
                say("CLASSIFICATION TIMED OUT. ENGAGE!", vec3(1.f, 0.4f, 0.2f), 5.f);
            }
            break;
        }
        case Stage::Respond:
        case Stage::Done: break;
    }
    if (w.phase == Phase::Running) {
        if (aliveDrones() == 0) finish(Phase::Success);
        else if (t > tImpact_ + 25.f) finish(Phase::Failure);
    }
}

// ---------------------------------------------------------------------------------------------
// Drones
// ---------------------------------------------------------------------------------------------
void Mission::updateDrones(float dt) {
    float t = std::max(0.f, w.threatTime);
    bool active = w.threatTime >= 0.f;
    float minDist = 1e9f;
    for (auto& s : w.swarms) {
        s.alive = 0;
        s.center = swarmCenter(s, t, &s.centerVel);
    }
    for (auto& d : w.drones) {
        Swarm& s = w.swarms[static_cast<size_t>(d.swarm)];
        d.hitFlash = std::max(0.f, d.hitFlash - dt * 4.f);
        d.rotorPhase += dt * (d.alive() ? 70.f : 30.f);
        if (d.state == Drone::Alive) {
            ++s.alive;
            vec3 prev = d.pos;
            float distPost = glm::length(s.center - vec3(0, kCabY, 0));
            float shrink = glm::clamp(distPost / 150.f, 0.08f, 1.f);
            vec3 wob = 1.2f * vec3(std::sin(t * 1.3f + d.seed), std::sin(t * 1.7f + d.seed * 2.f) * 0.6f, std::sin(t * 1.1f + d.seed * 3.f));
            d.pos = s.center + (d.offset + wob) * shrink;
            if (!active) d.pos = s.center + d.offset;
            vec3 v = (d.pos - prev) / std::max(dt, 1e-4f);
            if (dt < 1e-4f) v = s.centerVel;
            d.vel = glm::mix(d.vel, v, 0.2f);
            vec3 hv(d.vel.x, 0.f, d.vel.z);
            if (glm::length(d.vel) > 0.5f) {
                vec3 f = glm::normalize(d.vel);
                d.fwd = glm::normalize(glm::mix(d.fwd, f, glm::clamp(dt * 4.f, 0.f, 1.f)));
            }
            float lat = glm::dot(glm::cross(vec3(0, 1, 0), d.fwd), s.centerVel - d.vel * 0.0f) * 0.0f;
            (void)lat;
            d.bank = glm::mix(d.bank, glm::clamp(std::sin(t * 0.7f + d.seed) * 0.12f, -0.2f, 0.2f), 0.05f);
            float dd = glm::length(d.pos - vec3(0, kCabY, 0));
            minDist = std::min(minDist, dd);
            if (active && dd < 13.f && w.phase == Phase::Running) {
                explosion(d.pos, 3.5f, true);
                w.damage = 1.f;
                w.shake = 1.2f;
                w.jumpscare = true;
                finish(Phase::Failure);
            }
        } else if (d.state == Drone::Falling) {
            d.fallT += dt;
            d.vel.y -= 9.8f * dt;
            d.vel.x *= 1.f - 0.2f * dt;
            d.vel.z *= 1.f - 0.2f * dt;
            d.pos += d.vel * dt;
            d.bank += dt * 6.f;
            if (rng_.chance(dt * 30.f)) puff(d.pos, vec3(0, 1, 0), 0.6f, 1.4f, vec4(0.1f, 0.1f, 0.1f, 0.6f), 1.8f, 0.8f);
            if (d.pos.y < terrain_.height(d.pos.x, d.pos.z) + 0.3f || d.fallT > 3.5f) {
                explosion(d.pos, 0.8f, false);
                d.state = Drone::Dead;
            }
        }
    }
    // proximity alarm for the cab (red pulse)
    float alarmTarget = (active && minDist < 450.f && w.phase == Phase::Running) ? 1.f - minDist / 450.f : 0.f;
    w.alarm = glm::mix(w.alarm, alarmTarget, glm::clamp(dt * 3.f, 0.f, 1.f));
    w.damage = std::max(0.f, w.damage - dt * 0.35f);
}

void Mission::killDrone(Drone& d, bool byMissile) {
    if (d.state != Drone::Alive) return;
    d.state = Drone::Falling;
    d.hp = 0.f;
    d.fallT = 0.f;
    d.vel += vec3(rng_.gauss() * 1.5f, 1.5f, rng_.gauss() * 1.5f);
    ++kills_;
    if (byMissile) ++missileKillsTotal_;
    for (int i = 0; i < 6; ++i) spark(d.pos, rng_.unitSphere() * rng_.uniform(3.f, 10.f), 0.25f, vec4(1.f, 0.7f, 0.3f, 1.f), 0.5f);
    glow(d.pos, vec3(0), 1.4f, 4.f, vec4(1.f, 0.6f, 0.2f, 0.9f), 0.35f);
}

// ---------------------------------------------------------------------------------------------
// Player actions: tag / fire
// ---------------------------------------------------------------------------------------------
bool Mission::visibleTo(Item item, const vec3& p, float objSize, float zoom) const {
    vec3 eye = w.player.eye();
    float d = glm::length(p - eye);
    if (d < 1.f) return true;
    float fov = itemFovY(item, zoom);
    float pxAng = fov / 720.f;
    float px = (objSize / d) / pxAng;
    bool thermal = item == Item::Thermal;
    float T = std::exp(-env_.fogDensity * (thermal ? 0.22f : 1.f) * d);
    SensorState st = thermal ? scene_.thermal : scene_.binoculars;
    float minPx = 2.0f, minT = thermal ? 0.03f : 0.045f;
    if (st == SensorState::Degraded) {
        minPx = 3.5f;
        minT = thermal ? 0.06f : 0.10f;
    }
    if (px < minPx || T < minT) return false;
    return terrain_.lineOfSight(eye, p, 0.5f);
}

void Mission::falseAlarm(const char* what) {
    ++falseAlarms_;
    say(std::string("FALSE CONTACT: ") + what, vec3(1.f, 0.35f, 0.25f), 4.f);
}

void Mission::tagSwarm(int swarmId, const char* how) {
    Swarm& s = w.swarms[static_cast<size_t>(swarmId)];
    bool already = s.detected;
    s.detected = true;
    for (auto& d : w.drones)
        if (d.swarm == swarmId && d.alive()) d.tagged = true;
    if (w.stage == Stage::Detect && w.threatTime >= 0.f) {
        detectedByPlayer_ = true;
        detectTime_ = w.threatTime;
        w.stage = Stage::Classify;
        w.stageStart = w.threatTime;
        char buf[128];
        std::snprintf(buf, sizeof(buf), "CONTACT TAGGED (%s) BRG %03d. CLASSIFY: 1 SINGLE  2 SWARM  3 MULTI-SWARM", how,
                      static_cast<int>(degrees(s.bearing) + 0.5f) % 360);
        say(buf, vec3(0.5f, 1.f, 0.5f), 7.f);
    } else if (!already) {
        char buf[96];
        std::snprintf(buf, sizeof(buf), "TRACK ACQUIRED (%s) BRG %03d", how, static_cast<int>(degrees(s.bearing) + 0.5f) % 360);
        say(buf, vec3(0.5f, 1.f, 0.5f), 4.f);
    } else {
        say("TRACK UPDATED", vec3(0.5f, 1.f, 0.5f), 2.f);
    }
}

void Mission::tagVisual(Item item) {
    const Player& p = w.player;
    vec3 eye = p.eye();
    vec3 dir = aimDir();
    float zoom = p.zoom[static_cast<int>(item)];
    float fov = itemFovY(item, zoom);
    float tol = std::max(0.0025f, 0.05f * fov);
    auto angleTo = [&](const vec3& q) { return std::acos(glm::clamp(glm::dot(dir, glm::normalize(q - eye)), -1.f, 1.f)); };
    const Drone* best = nullptr;
    float bestA = tol;
    if (w.threatTime >= 0.f) {
        for (const auto& d : w.drones) {
            if (!d.alive()) continue;
            float a = angleTo(d.pos);
            if (a < bestA && visibleTo(item, d.pos, 0.9f, zoom)) {
                best = &d;
                bestA = a;
            }
        }
    }
    if (best) {
        tagSwarm(best->swarm, item == Item::Thermal ? "THERMAL" : "OPTICAL");
        return;
    }
    for (const auto& b : w.birds) {
        if (angleTo(b.pos) < tol * 1.6f && visibleTo(item, b.pos, 0.6f, zoom)) {
            falseAlarm("BIRDS");
            return;
        }
    }
    for (const auto& bl : w.blasts) {
        if (bl.active && angleTo(bl.pos + vec3(0, 12, 0)) < tol * 4.f) {
            falseAlarm("EXPLOSION / DECOY");
            return;
        }
    }
    say("NO CONTACT", vec3(0.7f, 0.8f, 0.7f), 1.5f);
}

void Mission::tagSensor(Item item) {
    const auto& list = item == Item::Acoustic ? w.acoustic : w.rf;
    float tolDeg;
    if (item == Item::Acoustic) tolDeg = scene_.audio == AudioLevel::High ? 7.f : (scene_.audio == AudioLevel::Medium ? 11.f : 17.f);
    else tolDeg = scene_.signal == SensorState::Available ? 7.f : 12.f;
    float yaw = aimYaw();
    const Contact* best = nullptr;
    float ba = radians(tolDeg);
    for (const auto& c : list) {
        float a = std::fabs(wrapAngle(c.bearing - yaw));
        if (a < ba && c.strength > 0.05f) {
            ba = a;
            best = &c;
        }
    }
    if (!best) {
        say("NO CONTACT IN SECTOR", vec3(0.7f, 0.8f, 0.7f), 1.5f);
        return;
    }
    if (best->source >= 0) tagSwarm(best->source, item == Item::Acoustic ? "ACOUSTIC" : "RF LINK");
    else falseAlarm(item == Item::Acoustic ? "ACOUSTIC NOISE" : "RF NOISE");
}

void Mission::action(const Input& in) {
    if (!in.space) return;
    Player& p = w.player;
    if (!p.inUse || p.raise < 0.85f || p.itemSwitchT > 0.5f) {
        say("RAISE ITEM FIRST (LEFT CLICK)", vec3(0.7f, 0.8f, 0.7f), 1.5f);
        return;
    }
    switch (p.item) {
        case Item::Binoculars:
        case Item::Thermal:
            if (!itemUsable(p.item)) say("DEVICE OFFLINE", vec3(1.f, 0.4f, 0.3f), 2.f);
            else tagVisual(p.item);
            break;
        case Item::Acoustic: tagSensor(Item::Acoustic); break;
        case Item::Rf:
            if (!itemUsable(p.item)) say("NO CARRIER", vec3(1.f, 0.4f, 0.3f), 2.f);
            else tagSensor(Item::Rf);
            break;
        case Item::Rifle: fireRifle(); break;
        case Item::Launcher: fireLauncher(); break;
        default: break;
    }
}

void Mission::fireRifle() {
    Player& p = w.player;
    if (p.rifleReload > 0.f || p.rifleCooldown > 0.f) return;
    if (p.rifleMag <= 0) {
        if (p.rifleReserve > 0) {
            p.rifleReload = 2.4f;
            say("RELOADING", vec3(0.8f, 0.8f, 0.5f), 2.f);
        } else say("OUT OF AMMO", vec3(1.f, 0.4f, 0.3f), 2.f);
        return;
    }
    --p.rifleMag;
    ++shots_;
    p.rifleCooldown = 0.38f;
    p.recoil += 0.012f;
    w.shake = std::max(w.shake, 0.08f);
    if (firstShotAt_ < 0.f) firstShotAt_ = w.threatTime;
    float spread = 0.0006f * (1.f + 1.5f * p.recoil * 20.f);
    vec3 d = aimDir();
    vec3 right = glm::normalize(glm::cross(d, vec3(0, 1, 0)));
    vec3 up = glm::cross(right, d);
    d = glm::normalize(d + right * rng_.gauss() * spread + up * rng_.gauss() * spread);
    Bullet b;
    b.pos = p.eye() + d * 0.8f - vec3(0.f, 0.05f, 0.f);
    b.vel = d * 880.f;
    b.life = 2.5f;
    b.alive = true;
    w.bullets.push_back(b);
    glow(b.pos + d * 0.3f, vec3(0), 0.6f, 6.f, vec4(1.f, 0.8f, 0.4f, 0.9f), 0.05f);
    w.flash = std::max(w.flash, 0.03f);
    if (p.rifleMag == 0 && p.rifleReserve > 0) {
        p.rifleReload = 2.4f;
        say("RELOADING", vec3(0.8f, 0.8f, 0.5f), 2.f);
    }
}

void Mission::updateLock(float dt) {
    Player& p = w.player;
    bool seeking = p.item == Item::Launcher && p.inUse && p.raise > 0.85f && p.launcherReload <= 0.f && p.missiles > 0;
    int best = -1;
    if (seeking && w.threatTime >= 0.f) {
        vec3 eye = p.eye();
        vec3 dir = aimDir();
        float bestA = radians(3.2f);
        for (const auto& d : w.drones) {
            if (!d.alive() || !d.tagged) continue;
            vec3 to = d.pos - eye;
            float dist = glm::length(to);
            if (dist > 2200.f) continue;
            float a = std::acos(glm::clamp(glm::dot(dir, to / dist), -1.f, 1.f));
            if (a >= bestA) continue;
            if (std::exp(-env_.fogDensity * dist) < 0.12f) continue;
            if (!terrain_.lineOfSight(eye, d.pos, 0.5f)) continue;
            bestA = a;
            best = d.id;
        }
    }
    if (best >= 0 && best == p.lockTarget) {
        float need = env_.visRange < 800.f ? 2.2f : 1.4f;
        p.lockTimer += dt;
        p.locked = p.lockTimer >= need;
    } else {
        p.lockTarget = best;
        p.lockTimer = 0.f;
        p.locked = false;
    }
}

void Mission::fireLauncher() {
    Player& p = w.player;
    if (p.launcherReload > 0.f) return;
    if (p.missiles <= 0) {
        say("NO MISSILES LEFT", vec3(1.f, 0.4f, 0.3f), 2.f);
        return;
    }
    --p.missiles;
    ++missilesFired_;
    p.launcherReload = 4.0f;
    p.recoil += 0.03f;
    w.shake = std::max(w.shake, 0.35f);
    if (firstShotAt_ < 0.f) firstShotAt_ = w.threatTime;
    Missile m;
    vec3 d = aimDir();
    m.pos = p.eye() + d * 1.2f - vec3(0.f, 0.1f, 0.f);
    m.vel = d * 45.f;
    m.target = p.locked ? p.lockTarget : -1;
    m.alive = true;
    w.missiles.push_back(m);
    for (int i = 0; i < 14; ++i) {
        puff(m.pos - d * 0.5f, -d * rng_.uniform(2.f, 9.f) + rng_.unitSphere() * 2.f, 0.7f, 3.f, vec4(0.8f, 0.8f, 0.8f, 0.5f), 2.5f, 0.7f);
    }
    glow(m.pos, vec3(0), 2.0f, 8.f, vec4(1.f, 0.7f, 0.3f, 0.9f), 0.2f);
    w.flash = std::max(w.flash, 0.1f);
    say(p.locked ? "MISSILE AWAY (LOCKED)" : "MISSILE AWAY (NO LOCK!)", p.locked ? vec3(0.6f, 1.f, 0.6f) : vec3(1.f, 0.6f, 0.2f), 3.f);
    p.locked = false;
    p.lockTimer = 0.f;
    p.lockTarget = -1;
}

void Mission::detonateMissile(Missile& m) {
    m.alive = false;
    explosion(m.pos, 2.4f, true);
    int killed = 0;
    for (auto& d : w.drones) {
        if (!d.alive()) continue;
        float dist = glm::length(d.pos - m.pos);
        float dmg = 0.f;
        if (dist <= 9.f) dmg = 3.f;
        else if (dist <= 18.f) dmg = 1.0f;
        if (dmg <= 0.f) continue;
        d.hp -= dmg;
        d.hitFlash = 1.f;
        if (d.hp <= 0.f) {
            killDrone(d, true);
            ++killed;
        }
    }
    if (killed > 0) ++missileEffective_;
    if (killed > 0) {
        char buf[64];
        std::snprintf(buf, sizeof(buf), "MISSILE IMPACT: %d DOWN", killed);
        say(buf, vec3(0.6f, 1.f, 0.6f), 3.f);
    } else {
        say("MISSILE MISSED", vec3(1.f, 0.6f, 0.2f), 3.f);
    }
}

void Mission::updateWeapons(float dt, const Input& in) {
    Player& p = w.player;
    p.rifleCooldown = std::max(0.f, p.rifleCooldown - dt);
    p.launcherReload = std::max(0.f, p.launcherReload - dt);
    if (p.rifleReload > 0.f) {
        p.rifleReload -= dt;
        if (p.rifleReload <= 0.f) {
            int need = 10 - p.rifleMag;
            int take = std::min(need, p.rifleReserve);
            p.rifleMag += take;
            p.rifleReserve -= take;
            p.rifleReload = 0.f;
        }
    }
    if (in.reload && p.rifleReload <= 0.f && p.rifleMag < 10 && p.rifleReserve > 0) {
        p.rifleReload = 2.4f;
        say("RELOADING", vec3(0.8f, 0.8f, 0.5f), 2.f);
    }
    action(in);
    updateLock(dt);

    // bullets
    vec3 eye = p.eye();
    for (auto& b : w.bullets) {
        if (!b.alive) continue;
        const int steps = 6;
        float ds = dt / steps;
        vec3 start = b.pos;
        for (int s = 0; s < steps && b.alive; ++s) {
            vec3 p0 = b.pos;
            b.vel.y -= 9.81f * ds;
            vec3 p1 = p0 + b.vel * ds;
            for (auto& d : w.drones) {
                if (!d.alive()) continue;
                float r = 1.0f + 0.003f * glm::length(d.pos - eye);
                if (segSphere(p0, p1, d.pos, r)) {
                    ++hits_;
                    d.hp -= 1.f;
                    d.hitFlash = 1.f;
                    for (int k = 0; k < 4; ++k) spark(d.pos, rng_.unitSphere() * rng_.uniform(2.f, 7.f), 0.18f, vec4(1.f, 0.8f, 0.4f, 1.f), 0.35f);
                    if (d.hp <= 0.f) killDrone(d, false);
                    b.alive = false;
                    break;
                }
            }
            if (!b.alive) break;
            b.pos = p1;
            if (b.pos.y < terrain_.height(b.pos.x, b.pos.z)) {
                for (int k = 0; k < 4; ++k) puff(b.pos, vec3(0, 3, 0) + rng_.unitSphere() * 2.f, 0.8f, 2.f, vec4(0.45f, 0.4f, 0.3f, 0.5f), 1.2f, 0.5f);
                b.alive = false;
            }
        }
        Tracer t;
        t.a = start;
        t.b = b.pos;
        w.tracers.push_back(t);
        b.life -= dt;
        if (b.life <= 0.f) b.alive = false;
    }
    w.bullets.erase(std::remove_if(w.bullets.begin(), w.bullets.end(), [](const Bullet& b) { return !b.alive; }), w.bullets.end());

    // missiles
    for (auto& m : w.missiles) {
        if (!m.alive) continue;
        m.life += dt;
        float speed = 45.f + 200.f * (1.f - std::exp(-m.life * 1.1f));
        vec3 dir = glm::length(m.vel) > 1e-3f ? glm::normalize(m.vel) : vec3(0, 0, -1);
        Drone* tgt = nullptr;
        if (m.target >= 0) {
            for (auto& d : w.drones)
                if (d.id == m.target && d.alive()) tgt = &d;
            if (!tgt) {  // retarget the nearest drone within 120 m
                float bd = 120.f;
                for (auto& d : w.drones) {
                    if (!d.alive()) continue;
                    float l = glm::length(d.pos - m.pos);
                    if (l < bd) { bd = l; tgt = &d; m.target = d.id; }
                }
                if (!tgt) m.target = -1;
            }
        }
        if (tgt) {
            float dist = glm::length(tgt->pos - m.pos);
            vec3 aim = tgt->pos + tgt->vel * (dist / std::max(speed, 1.f));
            vec3 want = glm::normalize(aim - m.pos);
            float maxTurn = 2.8f * dt;
            float ang = std::acos(glm::clamp(glm::dot(dir, want), -1.f, 1.f));
            if (ang > 1e-4f) dir = glm::normalize(glm::mix(dir, want, glm::clamp(maxTurn / ang, 0.f, 1.f)));
        } else {
            dir.y -= 0.02f * dt;
            dir = glm::normalize(dir);
        }
        m.vel = dir * speed;
        m.pos += m.vel * dt;
        m.trail -= dt;
        if (m.trail <= 0.f) {
            m.trail = 0.015f;
            puff(m.pos - dir * 0.7f, rng_.unitSphere() * 0.6f, 0.5f, 2.4f, vec4(0.85f, 0.85f, 0.85f, 0.55f), 3.5f, 0.75f, 0.4f);
            glow(m.pos - dir * 0.8f, vec3(0), 0.8f, 0.f, vec4(1.f, 0.7f, 0.3f, 0.9f), 0.06f);
        }
        bool boom = false;
        for (auto& d : w.drones)
            if (d.alive() && glm::length(d.pos - m.pos) < 7.f) boom = true;
        if (m.life > 9.f || m.pos.y < terrain_.height(m.pos.x, m.pos.z) + 0.5f) boom = true;
        if (boom) detonateMissile(m);
    }
    w.missiles.erase(std::remove_if(w.missiles.begin(), w.missiles.end(), [](const Missile& m) { return !m.alive; }), w.missiles.end());
    for (auto& t : w.tracers) t.age += dt;
    w.tracers.erase(std::remove_if(w.tracers.begin(), w.tracers.end(), [](const Tracer& t) { return t.age > 0.12f; }), w.tracers.end());
}

// ---------------------------------------------------------------------------------------------
// Sensors
// ---------------------------------------------------------------------------------------------
void Mission::updateSensors(float dt) {
    w.acousticJam = std::max(0.f, w.acousticJam - dt);
    w.rfJam = std::max(0.f, w.rfJam - dt);
    w.acoustic.clear();
    w.rf.clear();
    float t = w.time;
    bool active = w.threatTime >= 0.f;
    float aRange = scene_.audio == AudioLevel::High ? 1700.f : (scene_.audio == AudioLevel::Medium ? 950.f : 480.f);
    float aSigma = scene_.audio == AudioLevel::High ? 2.5f : (scene_.audio == AudioLevel::Medium ? 7.f : 16.f);
    if (scene_.weather == Weather::Rain) aRange *= 0.75f;
    if (scene_.weather == Weather::Storm) aRange *= 0.55f;
    float rRange = scene_.signal == SensorState::Available ? 3000.f : (scene_.signal == SensorState::Degraded ? 1500.f : 0.f);
    float rSigma = scene_.signal == SensorState::Available ? 3.f : 9.f;

    if (active) {
        for (const auto& s : w.swarms) {
            if (s.alive <= 0) continue;
            vec3 c = s.center;
            float range = std::sqrt(c.x * c.x + c.z * c.z);
            float trueB = bearingOf(c);
            float countF = 0.6f + 0.4f * std::sqrt(static_cast<float>(s.alive) / 10.f);
            if (w.acousticJam <= 0.f) {
                float st = std::pow(glm::clamp(1.f - range / aRange, 0.f, 1.f), 0.7f) * countF;
                if (st > 0.02f) {
                    Contact ct;
                    ct.source = s.id;
                    ct.range = range;
                    ct.strength = st;
                    ct.trueBearing = trueB;
                    ct.bearing = trueB + radians(aSigma) * smoothNoise(t * 0.9f, static_cast<float>(s.id) + 1.f);
                    w.acoustic.push_back(ct);
                }
            }
            if (w.rfJam <= 0.f && rRange > 0.f) {
                float st = std::sqrt(glm::clamp(1.f - range / rRange, 0.f, 1.f));
                if (scene_.signal == SensorState::Degraded) st *= smoothNoise(t * 1.7f, static_cast<float>(s.id) * 3.f + 2.f) > -0.35f ? 1.f : 0.f;
                if (st > 0.02f) {
                    Contact ct;
                    ct.source = s.id;
                    ct.range = range;
                    ct.strength = st;
                    ct.freq = s.freq;
                    ct.trueBearing = trueB;
                    ct.bearing = trueB + radians(rSigma) * smoothNoise(t * 1.3f, static_cast<float>(s.id) + 7.f);
                    w.rf.push_back(ct);
                }
            }
        }
    }
    // decoy contacts
    for (auto& n : noiseEvts_) {
        if (w.missionTime > n.until) continue;
        Contact ct;
        ct.source = -1;
        ct.range = 400.f;
        ct.strength = n.strength * (0.7f + 0.3f * smoothNoise(t * 4.f, n.bearing));
        ct.trueBearing = n.bearing;
        ct.bearing = n.bearing + radians(aSigma * 0.5f) * smoothNoise(t, n.bearing);
        if (w.acousticJam <= 0.f) w.acoustic.push_back(ct);
        if (rRange > 0.f && n.strength > 0.55f && w.rfJam <= 0.f) {
            ct.freq = 2.44f + 0.01f * std::fmod(n.bearing * 10.f, 3.f);
            ct.strength *= 0.7f;
            w.rf.push_back(ct);
        }
    }
    noiseEvts_.erase(std::remove_if(noiseEvts_.begin(), noiseEvts_.end(), [&](const NoiseEvt& n) { return w.missionTime > n.until; }), noiseEvts_.end());
    for (const auto& b : w.blasts) {
        if (!b.active || b.age > 6.f) continue;
        Contact ct;
        ct.source = -1;
        ct.range = glm::length(vec2(b.pos.x, b.pos.z));
        ct.strength = 1.f - b.age / 6.f;
        ct.trueBearing = ct.bearing = bearingOf(b.pos);
        w.acoustic.push_back(ct);
        if (rRange > 0.f) {
            ct.freq = 2.46f;
            w.rf.push_back(ct);
        }
    }

    // waterfall (RF) + waveform (acoustic) history
    nextWave_ -= dt;
    if (nextWave_ <= 0.f) {
        nextWave_ = 1.f / 14.f;
        std::array<uint8_t, 128> row;
        for (int i = 0; i < 128; ++i) {
            float f = 22.f + 18.f * hash01(static_cast<uint32_t>(i) * 7919u + static_cast<uint32_t>(t * 14.f) * 104729u);
            if (scene_.signal == SensorState::Unavailable) f = 8.f;
            row[static_cast<size_t>(i)] = static_cast<uint8_t>(f);
        }
        if (scene_.signal != SensorState::Unavailable) {
            for (const auto& c : w.rf) {
                float centre = (c.freq - 2.400f) / 0.100f * 127.f;
                for (int i = 0; i < 128; ++i) {
                    float d = (i - centre);
                    float v = c.strength * 230.f * std::exp(-d * d / 3.2f);
                    row[static_cast<size_t>(i)] = static_cast<uint8_t>(std::min(255.f, row[static_cast<size_t>(i)] + v));
                }
            }
            if (w.rfJam > 0.f) {
                for (int i = 0; i < 128; ++i)
                    row[static_cast<size_t>(i)] = static_cast<uint8_t>(std::min(255.f, row[static_cast<size_t>(i)] + 120.f * w.rfJam + 80.f * hash01(static_cast<uint32_t>(i) + static_cast<uint32_t>(t * 30.f))));
            }
        }
        w.waterfall.push_back(row);
        while (w.waterfall.size() > 72) w.waterfall.pop_front();
        float amp = 0.05f + 0.04f * hash01(static_cast<uint32_t>(t * 14.f));
        for (const auto& c : w.acoustic) amp = std::max(amp, c.strength * (0.8f + 0.2f * hash01(static_cast<uint32_t>(t * 28.f))));
        if (w.acousticJam > 0.f) amp = 1.f;
        w.audioWave.push_back(amp);
        while (w.audioWave.size() > 120) w.audioWave.pop_front();
    }
}

// ---------------------------------------------------------------------------------------------
// Decoys (birds / blasts / noise)
// ---------------------------------------------------------------------------------------------
void Mission::updateDecoys(float dt) {
    if (w.phase != Phase::Running) {
        // existing birds keep flying, nothing new is scheduled
    } else {
        if (scene_.decoys == Decoys::Noise && w.threatTime > -quiet_ + 1.f) {
            nextFlock_ -= dt;
            if (nextFlock_ <= 0.f) {
                nextFlock_ = rng_.uniform(7.f, 15.f);
                int n = rng_.range(3, 9);
                float b = rng_.uniform(0.f, kTwoPi);
                float dist = rng_.uniform(120.f, 420.f);
                vec3 dir = dirOfBearing(b);
                vec3 tang(std::cos(b), 0.f, std::sin(b));
                if (rng_.chance(0.5f)) tang = -tang;
                float speed = rng_.uniform(9.f, 14.f);
                float alt = rng_.uniform(22.f, 70.f);
                vec3 start = dir * dist - tang * 350.f;
                for (int i = 0; i < n; ++i) {
                    Bird bd;
                    bd.pos = start + vec3(rng_.uniform(-12.f, 12.f), alt + rng_.uniform(-6.f, 6.f), rng_.uniform(-12.f, 12.f));
                    bd.vel = tang * (speed + rng_.uniform(-1.f, 1.f));
                    bd.phase = rng_.uniform(0.f, kTwoPi);
                    bd.life = 700.f / speed;
                    w.birds.push_back(bd);
                }
            }
            nextNoise_ -= dt;
            if (nextNoise_ <= 0.f) {
                nextNoise_ = rng_.uniform(3.f, 7.f);
                NoiseEvt e;
                e.bearing = rng_.uniform(0.f, kTwoPi);
                e.until = w.missionTime + rng_.uniform(2.f, 5.f);
                e.strength = rng_.uniform(0.35f, 0.85f);
                noiseEvts_.push_back(e);
            }
        }
        if (scene_.decoys == Decoys::Blast && !blastSchedule_.empty()) {
            for (size_t i = 0; i < blastSchedule_.size();) {
                if (w.threatTime >= blastSchedule_[i]) {
                    float b = 0.f;
                    for (int tries = 0; tries < 20; ++tries) {
                        b = rng_.uniform(0.f, kTwoPi);
                        bool ok = true;
                        for (const auto& s : w.swarms)
                            if (std::fabs(wrapAngle(b - s.bearing)) < radians(30.f)) ok = false;
                        if (ok) break;
                    }
                    float dist = rng_.uniform(600.f, 1400.f);
                    vec3 p = dirOfBearing(b) * dist;
                    p.y = terrain_.height(p.x, p.z) + 1.f;
                    Blast bl;
                    bl.pos = p;
                    bl.active = true;
                    w.blasts.push_back(bl);
                    explosion(p, 9.f, false);
                    w.flash = std::max(w.flash, 0.75f);
                    w.acousticJam = 3.f;
                    w.rfJam = 2.f;
                    w.flicker = 1.f;
                    w.shake = std::max(w.shake, 0.35f);
                    say("EXPLOSION REPORTED - CHECK YOUR SECTOR", vec3(1.f, 0.7f, 0.3f), 4.f);
                    blastSchedule_.erase(blastSchedule_.begin() + static_cast<long>(i));
                } else {
                    ++i;
                }
            }
        }
    }
    for (auto& b : w.birds) {
        b.pos += b.vel * dt;
        b.pos.y += std::sin(w.time * 2.f + b.phase) * 0.8f * dt;
        b.phase += dt * 9.f;
        b.life -= dt;
    }
    w.birds.erase(std::remove_if(w.birds.begin(), w.birds.end(), [](const Bird& b) { return b.life <= 0.f; }), w.birds.end());
    for (auto& bl : w.blasts) {
        if (!bl.active) continue;
        bl.age += dt;
        bl.smokeT -= dt;
        if (bl.smokeT <= 0.f && bl.age < 28.f) {
            bl.smokeT = 0.1f;
            puff(bl.pos + vec3(rng_.gauss() * 4.f, rng_.uniform(0.f, 6.f), rng_.gauss() * 4.f), vec3(rng_.gauss(), rng_.uniform(5.f, 9.f), rng_.gauss()),
                 9.f, 6.f, vec4(0.08f, 0.07f, 0.07f, 0.8f), 9.f, 0.6f, 0.1f);
            if (bl.age < 10.f)
                glow(bl.pos + vec3(rng_.gauss() * 3.f, rng_.uniform(0.f, 5.f), rng_.gauss() * 3.f), vec3(0, 4, 0), 6.f, 2.f, vec4(1.f, 0.5f, 0.15f, 0.8f), 0.8f);
        }
        if (bl.age > 30.f) bl.active = false;
    }
}

void Mission::updateEnvironmentEvents(float dt) {
    w.flash = std::max(0.f, w.flash - dt * 2.6f);
    float dist = 0.f;
    (void)dist;
    if (env_.storm > 0.5f) {
        nextLightning_ -= dt;
        if (nextLightning_ <= 0.f) {
            nextLightning_ = rng_.uniform(4.f, 11.f);
            w.flash = 1.f;
            thunderAt_ = w.missionTime + 0.16f;
            w.flicker = 1.f;
        }
        if (thunderAt_ > 0.f && w.missionTime >= thunderAt_) {
            thunderAt_ = -1.f;
            w.flash = std::max(w.flash, 0.7f);
        }
    }
    // cab light flicker (decays back to a faint hum)
    static float burst = 0.f;
    burst = std::max(burst, w.flicker);
    w.flicker = 0.f;
    burst = std::max(0.f, burst - dt * 0.9f);
    float hum = hash01(static_cast<uint32_t>(w.time * 11.f) + 17u) > 0.82f ? 0.06f : 0.f;
    float b = burst > 0.f ? (hash01(static_cast<uint32_t>(w.time * 32.f) + 3u) > 0.45f ? 0.55f * std::min(1.f, burst * 2.f) : 0.f) : 0.f;
    bool noiseActive = !noiseEvts_.empty();
    float nz = noiseActive && hash01(static_cast<uint32_t>(w.time * 20.f) + 91u) > 0.93f ? 0.35f : 0.f;
    w.flicker = std::max(std::max(hum, b), nz);
    // flicker value consumed by the renderer this frame
}

// ---------------------------------------------------------------------------------------------
// Effects
// ---------------------------------------------------------------------------------------------
void Mission::puff(const vec3& p, const vec3& v, float size, float grow, const vec4& col, float life, float heat, float drag) {
    if (w.particles.size() > 5000) return;
    Particle q;
    q.pos = p;
    q.vel = v;
    q.size = size;
    q.grow = grow;
    q.color = col;
    q.life = q.maxLife = life;
    q.type = 0;
    q.heat = heat;
    q.drag = drag;
    q.rot = rng_.uniform(0.f, kTwoPi);
    q.rotVel = rng_.uniform(-0.6f, 0.6f);
    w.particles.push_back(q);
}

void Mission::glow(const vec3& p, const vec3& v, float size, float grow, const vec4& col, float life, float heat) {
    if (w.particles.size() > 5000) return;
    Particle q;
    q.pos = p;
    q.vel = v;
    q.size = size;
    q.grow = grow;
    q.color = col;
    q.life = q.maxLife = life;
    q.type = 1;
    q.heat = heat;
    q.additive = true;
    q.rot = rng_.uniform(0.f, kTwoPi);
    w.particles.push_back(q);
}

void Mission::spark(const vec3& p, const vec3& v, float size, const vec4& col, float life) {
    if (w.particles.size() > 5000) return;
    Particle q;
    q.pos = p;
    q.vel = v;
    q.size = size;
    q.color = col;
    q.life = q.maxLife = life;
    q.type = 2;
    q.heat = 1.f;
    q.additive = true;
    q.gravity = 9.8f;
    q.drag = 0.6f;
    w.particles.push_back(q);
}

void Mission::explosion(const vec3& p, float scale, bool flashy) {
    for (int i = 0; i < 9; ++i)
        glow(p + rng_.unitSphere() * scale * 0.8f, rng_.unitSphere() * scale * 2.5f, scale * rng_.uniform(1.6f, 2.8f), scale * 1.5f,
             vec4(1.f, rng_.uniform(0.35f, 0.6f), 0.1f, 0.95f), rng_.uniform(0.45f, 0.9f));
    glow(p, vec3(0), scale * 3.5f, scale * 4.f, vec4(1.f, 0.85f, 0.6f, 1.f), 0.22f);
    for (int i = 0; i < 12; ++i)
        puff(p + rng_.unitSphere() * scale, rng_.unitSphere() * scale * 1.8f + vec3(0, scale * 1.5f, 0), scale * rng_.uniform(1.4f, 2.6f), scale * 0.9f,
             vec4(0.12f, 0.11f, 0.10f, 0.75f), rng_.uniform(3.f, 6.f), 0.65f, 0.7f);
    for (int i = 0; i < 12; ++i)
        spark(p, rng_.unitSphere() * rng_.uniform(8.f, 22.f) * std::min(scale, 3.f), 0.25f, vec4(1.f, 0.7f, 0.3f, 1.f), rng_.uniform(0.5f, 1.2f));
    if (flashy) {
        float d = glm::length(p - w.player.eye());
        w.flash = std::max(w.flash, glm::clamp(0.9f * scale * 60.f / (d + 60.f) / 3.f, 0.05f, 0.9f));
    }
}

void Mission::updateEffects(float dt) {
    for (auto& p : w.particles) {
        p.pos += p.vel * dt;
        p.vel *= std::exp(-p.drag * dt);
        p.vel.y -= p.gravity * dt;
        p.size += p.grow * dt;
        p.rot += p.rotVel * dt;
        p.life -= dt;
    }
    w.particles.erase(std::remove_if(w.particles.begin(), w.particles.end(), [](const Particle& p) { return p.life <= 0.f; }), w.particles.end());
}

// ---------------------------------------------------------------------------------------------
// Outcome and report
// ---------------------------------------------------------------------------------------------
void Mission::finish(Phase outcome) {
    if (w.phase != Phase::Running) return;
    w.phase = outcome;
    w.endTimer = 0.f;
    computeResult(outcome == Phase::Success);
    switch (outcome) {
        case Phase::Success:
            w.outcomeTitle = "THREAT NEUTRALIZED";
            w.outcomeSub = "SHIFT COMPLETE";
            break;
        case Phase::Failure:
            w.outcomeTitle = "POST DESTROYED";
            w.outcomeSub = "YOU DID NOT MAKE IT TO THE END OF THE SHIFT";
            break;
        default:
            w.outcomeTitle = "MISSION ABORTED";
            w.outcomeSub = "";
            finished_ = true;
            break;
    }
}

void Mission::computeResult(bool completed) {
    if (resultComputed_) return;
    resultComputed_ = true;
    MissionResult& r = result_;
    float tNow = std::max(0.f, w.threatTime);
    float det = detectedByPlayer_ ? detectTime_ : Td_;
    float cls = (classDoneAt_ >= 0.f) ? (w.classTimedOut ? Tc_ : classTime_) : Tc_;
    float doneAt = classDoneAt_ >= 0.f ? classDoneAt_ : tNow;
    float resp;
    if (firstShotAt_ >= 0.f) resp = std::max(0.f, firstShotAt_ - doneAt);
    else resp = std::max(0.f, tNow - doneAt);
    r.detectionTime = det;
    r.classificationTime = cls;
    r.responseTime = resp;
    r.shotsFired = shots_;
    r.missilesFired = missilesFired_;
    r.completed = completed;

    float sd = detectedByPlayer_ ? stageScore(det / Td_) : 0.f;
    float sc = (classDoneAt_ >= 0.f && !w.classTimedOut) ? stageScore(cls / Tc_) : 0.f;
    float sr = firstShotAt_ >= 0.f ? stageScore(resp / Tr_) : 0.f;
    r.timeScore = 100.f * (0.35f * sd + 0.25f * sc + 0.40f * sr);

    int actual = scene_.threatType == ThreatType::Single ? 1 : (scene_.threatType == ThreatType::Swarm ? 2 : 3);
    float classPts = (w.classChoice == actual) ? 45.f : (w.classChoice != 0 ? 8.f : 0.f);
    float weaponPts;
    bool single = actual == 1;
    if (single) {
        weaponPts = missilesFired_ == 0 ? (shots_ > 0 ? 30.f : 0.f) : std::max(0.f, 30.f - 15.f * missilesFired_);
    } else {
        weaponPts = missilesFired_ > 0 ? std::min(30.f, 14.f + 8.f * std::min(missileEffective_, 2) + (missileKillsTotal_ > 0 ? 0.f : 0.f)) : 6.f;
        if (missilesFired_ > 0 && missileEffective_ == 0) weaponPts = 8.f;
    }
    float eff = 25.f * glm::clamp(1.f - 0.4f * falseAlarms_, 0.f, 1.f) * (completed ? 1.f : 0.4f);
    r.decisionScore = glm::clamp(classPts + weaponPts + eff, 0.f, 100.f);

    int attempts = shots_ + missilesFired_;
    r.accuracy = attempts > 0 ? 100.f * glm::clamp(static_cast<float>(hits_ + missileEffective_) / attempts, 0.f, 1.f) : 0.f;
    r.overallScore = glm::clamp((0.40f * r.decisionScore + 0.35f * r.timeScore + 0.25f * r.accuracy) * (completed ? 1.f : 0.55f), 0.f, 100.f);
    LOG_INFO("result: det=%.1f cls=%.1f resp=%.1f shots=%d missiles=%d decision=%.1f time=%.1f acc=%.1f overall=%.1f completed=%d", r.detectionTime,
             r.classificationTime, r.responseTime, r.shotsFired, r.missilesFired, r.decisionScore, r.timeScore, r.accuracy, r.overallScore, completed);
}

bool Mission::writePerformance(const std::string& path) const {
    jl::Writer j;
    j.beginObject();
    j.beginObject("mission");
    j.kv("id", scene_.missionId);
    j.kv("mode", scene_.mode);
    j.kv("difficulty_level", scene_.difficulty);
    j.endObject();
    j.beginObject("performance");
    j.beginObject("response");
    j.kv("detection_time_seconds", static_cast<double>(result_.detectionTime));
    j.kv("classification_time_seconds", static_cast<double>(result_.classificationTime));
    j.kv("response_time_seconds", static_cast<double>(result_.responseTime));
    j.endObject();
    j.beginObject("actions");
    j.kv("shots fired", result_.shotsFired);
    j.kv("missiles fired", result_.missilesFired);
    j.endObject();
    j.kv("decision_score", static_cast<double>(result_.decisionScore));
    j.kv("time_score", static_cast<double>(result_.timeScore));
    j.kv("accuracy", static_cast<double>(result_.accuracy));
    j.kv("overall_score", static_cast<double>(result_.overallScore));
    j.kv("mission_completed", result_.completed);
    j.endObject();
    j.endObject();
    return writeTextFileAtomic(path, j.str());
}

}  // namespace aegis
