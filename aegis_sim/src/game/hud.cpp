#include "game/hud.h"

#include <cstdio>

namespace aegis {

namespace {
const vec4 kGreen(0.45f, 1.0f, 0.55f, 0.95f);
const vec4 kDim(0.35f, 0.75f, 0.42f, 0.7f);
const vec4 kAmber(1.0f, 0.7f, 0.2f, 0.95f);
const vec4 kRed(1.0f, 0.25f, 0.15f, 0.95f);
const vec4 kPanel(0.02f, 0.05f, 0.03f, 0.62f);

std::string fmtTime(float s) {
    int t = static_cast<int>(std::max(0.f, s) + 0.99f);
    char b[16];
    std::snprintf(b, sizeof(b), "%02d:%02d", t / 60, t % 60);
    return b;
}

bool project(const mat4& vp, const vec3& p, const vec4& hole, vec2& out) {
    vec4 c = vp * vec4(p, 1.f);
    if (c.w <= 0.01f) return false;
    vec2 n = vec2(c.x, c.y) / c.w;
    if (std::fabs(n.x) > 1.2f || std::fabs(n.y) > 1.2f) return false;
    out.x = hole.x + (n.x * 0.5f + 0.5f) * (hole.z - hole.x);
    out.y = hole.y + (0.5f - n.y * 0.5f) * (hole.w - hole.y);
    return true;
}

std::string bearingStr(float b) {
    int d = static_cast<int>(degrees(b) + 0.5f) % 360;
    if (d < 0) d += 360;
    char buf[8];
    std::snprintf(buf, sizeof(buf), "%03d", d);
    return buf;
}
}  // namespace

void Hud::draw(Ui& ui, const Mission& m, const DeviceLayout& dev, float time) {
    const World& w = m.w;
    if (w.phase == Phase::Briefing) {
        briefing(ui, m, time);
        return;
    }
    if (dev.active) {
        if (dev.style == 2) {
            if (dev.item == Item::Acoustic) acousticTablet(ui, m, dev, time);
            else rfTablet(ui, m, dev, time);
        } else if (dev.hasImage) {
            reticle(ui, m, dev, time);
        }
    }
    overlays(ui, m, time);
    status(ui, m, time);
    compass(ui, m);
    messages(ui, m);
    hotbar(ui, m);
    if (w.phase == Phase::Success || w.phase == Phase::Failure) {
        if (w.jumpscare && w.endTimer < 1.8f) jumpscare(ui, m, time);
        else outcome(ui, m, time);
    }
}

void Hud::overlays(Ui& ui, const Mission& m, float time) {
    const World& w = m.w;
    float W = static_cast<float>(ui.width()), H = static_cast<float>(ui.height());
    if (w.alarm > 0.02f) {
        float pulse = 0.5f + 0.5f * std::sin(time * (4.f + 8.f * w.alarm));
        float a = w.alarm * pulse * 0.35f;
        float t = 14.f + 30.f * w.alarm;
        ui.rectGradV(0, 0, W, t, vec4(0.8f, 0.05f, 0.03f, a), vec4(0.8f, 0.05f, 0.03f, 0));
        ui.rectGradV(0, H - t, W, t, vec4(0.8f, 0.05f, 0.03f, 0), vec4(0.8f, 0.05f, 0.03f, a));
    }
    if (w.damage > 0.01f) ui.rect(0, 0, W, H, vec4(0.7f, 0.02f, 0.02f, w.damage * 0.55f));
    if (w.flash > 0.02f) ui.rect(0, 0, W, H, vec4(0.85f, 0.9f, 1.f, std::min(0.55f, w.flash * 0.5f)));
}

void Hud::status(Ui& ui, const Mission& m, float time) {
    const World& w = m.w;
    float x = 18.f, y = 16.f;
    float s = std::max(2.f, std::floor(ui.height() / 380.f));
    float pw = 300.f * s * 0.62f, ph = 56.f * s * 0.62f + 12;
    ui.rect(x - 8, y - 8, pw, ph + 40 * 0.0f, kPanel);
    ui.rectOutline(x - 8, y - 8, pw, ph, 1.f, vec4(0.3f, 0.7f, 0.4f, 0.5f));
    ui.text(x, y, m.clockText(), s, kGreen);
    // rec dot
    if (std::fmod(time, 1.2f) < 0.7f) ui.circle(x + pw - 30.f, y + 3.5f * s, 4.f * s * 0.5f, vec4(1.f, 0.1f, 0.05f, 0.9f));
    char buf[96];
    std::snprintf(buf, sizeof(buf), "POST-%02d  CAM %d", m.scene().missionId % 100, 1);
    ui.text(x, y + 9.f * s, buf, s * 0.6f, kDim);
    const char* stg = "STANDBY";
    vec4 col = kGreen;
    float rem = m.stageRemaining();
    float win = 1.f;
    int num = 0;
    if (w.phase == Phase::Running && w.threatTime >= 0.f) {
        switch (w.stage) {
            case Stage::Detect: stg = "DETECT"; num = 1; win = m.detectWindow(); break;
            case Stage::Classify: stg = "CLASSIFY"; num = 2; win = m.classifyWindow(); break;
            case Stage::Respond: stg = "ENGAGE"; num = 3; win = m.respondWindow(); break;
            default: break;
        }
        if (w.stage == Stage::Respond) win = std::max(1.f, m.impactTime() - w.stageStart);
        if (rem < 0.25f * win) col = kRed;
        else if (rem < 0.5f * win) col = kAmber;
    } else if (w.phase == Phase::Running) {
        stg = "SHIFT IN PROGRESS";
    }
    if (num > 0) std::snprintf(buf, sizeof(buf), "STAGE %d/3  %s  %s", num, stg, fmtTime(rem).c_str());
    else std::snprintf(buf, sizeof(buf), "%s", stg);
    ui.text(x, y + 17.f * s, buf, s * 0.65f, col);
    if (num > 0) {
        float bw = pw - 24.f;
        ui.rect(x, y + 27.f * s, bw, 5.f, vec4(0.f, 0.f, 0.f, 0.5f));
        ui.rect(x, y + 27.f * s, bw * glm::clamp(rem / win, 0.f, 1.f), 5.f, col);
    }
}

void Hud::compass(Ui& ui, const Mission& m) {
    float W = static_cast<float>(ui.width());
    float cx = W * 0.5f, y = 14.f;
    float s = std::max(1.5f, std::floor(ui.height() / 480.f));
    float width = std::min(W * 0.4f, 560.f);
    ui.rect(cx - width / 2, y, width, 20.f * s * 0.7f, vec4(0.02f, 0.05f, 0.03f, 0.55f));
    float yaw = degrees(m.aimYaw());
    float ppd = width / 90.f;  // pixels per degree
    float h = 20.f * s * 0.7f;
    for (int d = -60; d <= 60; ++d) {
        float deg = std::floor(yaw) + d;
        float x = cx + (deg - yaw) * ppd;
        if (x < cx - width / 2 + 4 || x > cx + width / 2 - 4) continue;
        int idx = static_cast<int>(deg);
        idx = ((idx % 360) + 360) % 360;
        if (idx % 10 == 0) {
            ui.line(x, y + h * 0.55f, x, y + h, 1.f, kDim);
            const char* lab = nullptr;
            char b[8];
            if (idx % 90 == 0) lab = idx == 0 ? "N" : (idx == 90 ? "E" : (idx == 180 ? "S" : "W"));
            else if (idx % 30 == 0) { std::snprintf(b, sizeof(b), "%d", idx); lab = b; }
            if (lab) ui.text(x, y + 2, lab, s * 0.7f, idx % 90 == 0 ? kAmber : kGreen, Align::Center);
        } else if (idx % 5 == 0) {
            ui.line(x, y + h * 0.75f, x, y + h, 1.f, kDim);
        }
    }
    ui.triangle(vec2(cx - 5, y + h + 1), vec2(cx + 5, y + h + 1), vec2(cx, y + h - 6), kAmber);
    ui.text(cx, y + h + 4, bearingStr(m.aimYaw()), s * 0.8f, kGreen, Align::Center);
    // marker for tagged swarms
    for (const auto& sw : m.w.swarms) {
        if (!sw.detected || sw.alive <= 0) continue;
        float d = wrapAngle(radians(degrees(sw.bearing)) - m.aimYaw());
        float x = cx + degrees(d) * ppd;
        if (std::fabs(x - cx) < width / 2 - 6) ui.triangle(vec2(x - 5, y - 2), vec2(x + 5, y - 2), vec2(x, y + 7), kRed);
    }
}

void Hud::messages(Ui& ui, const Mission& m) {
    float H = static_cast<float>(ui.height());
    float s = std::max(1.6f, std::floor(H / 520.f));
    float y = H - 28.f - 14.f * s * 4.0f;
    int n = 0;
    for (const auto& msg : m.w.messages) {
        float a = glm::clamp((msg.life - msg.age) / 0.8f, 0.f, 1.f) * glm::clamp(msg.age / 0.1f, 0.f, 1.f);
        vec4 c(msg.color, a);
        ui.rect(14.f, y - 2, ui.textWidth(msg.text, s) + 16.f, 9.f * s + 4.f, vec4(0.f, 0.f, 0.f, 0.45f * a));
        ui.text(22.f, y, msg.text, s, c);
        y += 11.f * s + 2.f;
        ++n;
    }
}

void Hud::hotbar(Ui& ui, const Mission& m) {
    const World& w = m.w;
    float W = static_cast<float>(ui.width()), H = static_cast<float>(ui.height());
    float s = std::max(1.5f, std::floor(H / 540.f));
    float bw = 112.f * s * 0.8f, bh = 24.f * s * 0.8f, gap = 4.f;
    float total = kItemCount * bw + (kItemCount - 1) * gap;
    float x0 = W - total - 16.f, y = H - bh - 14.f;
    for (int i = 0; i < kItemCount; ++i) {
        Item it = static_cast<Item>(i);
        bool sel = w.player.item == it;
        bool ok = m.itemUsable(it);
        float x = x0 + i * (bw + gap);
        ui.rect(x, y, bw, bh, vec4(0.02f, 0.05f, 0.03f, sel ? 0.8f : 0.5f));
        vec4 c = !ok ? vec4(0.5f, 0.2f, 0.2f, 0.8f) : (sel ? (w.player.inUse ? kAmber : kGreen) : kDim);
        ui.rectOutline(x, y, bw, bh, sel ? 2.f : 1.f, c);
        char b[8];
        std::snprintf(b, sizeof(b), "%d", i + 1);
        ui.text(x + 4, y + 3, b, s * 0.55f, c);
        std::string nm = itemName(it);
        if (nm.size() > 9) nm = nm.substr(0, nm.find(' '));
        ui.text(x + bw * 0.5f, y + bh * 0.5f - 3.5f * s * 0.55f, nm, s * 0.55f, c, Align::Center);
    }
    // weapon info / hint
    float ty = y - 14.f * s * 0.6f;
    char buf[128];
    if (w.player.item == Item::Rifle) {
        std::snprintf(buf, sizeof(buf), "AMMO %02d / %02d%s", w.player.rifleMag, w.player.rifleReserve, w.player.rifleReload > 0 ? "  RELOADING" : "");
        ui.text(W - 16.f, ty, buf, s * 0.7f, kGreen, Align::Right);
    } else if (w.player.item == Item::Launcher) {
        std::snprintf(buf, sizeof(buf), "MISSILES %d%s", w.player.missiles, w.player.launcherReload > 0 ? "  RELOADING" : "");
        ui.text(W - 16.f, ty, buf, s * 0.7f, kGreen, Align::Right);
    } else {
        std::snprintf(buf, sizeof(buf), "%s: %s", itemName(w.player.item), m.itemStatus(w.player.item).c_str());
        ui.text(W - 16.f, ty, buf, s * 0.7f, kGreen, Align::Right);
    }
    const char* hint = w.player.inUse ? "LMB: LOWER   RMB: NEXT ITEM   SPACE: ACTION   WHEEL: ZOOM   SHIFT: STEADY" : "LMB: RAISE   RMB: NEXT ITEM   1-3: CLASSIFY";
    ui.text(W * 0.5f, H - 18.f, hint, s * 0.55f, vec4(0.6f, 0.8f, 0.6f, 0.65f), Align::Center);
}

void Hud::reticle(Ui& ui, const Mission& m, const DeviceLayout& dev, float time) {
    const World& w = m.w;
    float x0 = dev.hole.x, y0 = dev.hole.y, x1 = dev.hole.z, y1 = dev.hole.w;
    float cx = (x0 + x1) * 0.5f, cy = (y0 + y1) * 0.5f;
    float hh = (y1 - y0);
    float s = std::max(1.2f, std::floor(hh / 420.f));
    bool thermal = dev.item == Item::Thermal;
    vec4 c = thermal ? vec4(1.f, 1.f, 1.f, 0.85f) : (m.env().nightness > 0.5f ? vec4(0.5f, 1.f, 0.5f, 0.85f) : vec4(0.f, 0.f, 0.f, 0.8f));
    if (dev.item == Item::Rifle || dev.item == Item::Launcher) c = (m.env().nightness > 0.5f) ? vec4(0.5f, 1.f, 0.5f, 0.85f) : vec4(0.02f, 0.02f, 0.02f, 0.9f);
    // scissor-less reticle limited to the hole by construction
    float r = dev.circle ? hh * 0.5f : std::min(x1 - x0, y1 - y0) * 0.5f;
    if (dev.item == Item::Rifle) {
        ui.line(cx - r * 0.95f, cy, cx - r * 0.06f, cy, 1.5f, c);
        ui.line(cx + r * 0.06f, cy, cx + r * 0.95f, cy, 1.5f, c);
        ui.line(cx, cy + r * 0.06f, cx, cy + r * 0.95f, 1.5f, c);
        ui.line(cx, cy - r * 0.95f, cx, cy - r * 0.06f, 3.f, c);
        for (int i = 1; i <= 8; ++i) {
            float d = i * r * 0.1f;
            ui.line(cx - 4 * s, cy + d, cx + 4 * s, cy + d, 1.5f, c);
            ui.line(cx - 4 * s, cy - d, cx + 4 * s, cy - d, 1.5f, c);
            ui.line(cx + d, cy - 4 * s, cx + d, cy + 4 * s, 1.5f, c);
            ui.line(cx - d, cy - 4 * s, cx - d, cy + 4 * s, 1.5f, c);
        }
    } else if (dev.item == Item::Launcher) {
        ui.ring(cx, cy, r * 0.12f, r * 0.12f + 1.5f, 0, kTwoPi, c);
        ui.line(cx - r * 0.35f, cy, cx - r * 0.14f, cy, 1.5f, c);
        ui.line(cx + r * 0.14f, cy, cx + r * 0.35f, cy, 1.5f, c);
        ui.line(cx, cy - r * 0.35f, cx, cy - r * 0.14f, 1.5f, c);
        ui.line(cx, cy + r * 0.14f, cx, cy + r * 0.35f, 1.5f, c);
        // seeker lock box
        const Player& p = w.player;
        float prog = p.locked ? 1.f : glm::clamp(p.lockTimer / 1.4f, 0.f, 1.f);
        if (p.lockTarget >= 0 || p.locked) {
            float bw = r * (0.5f - 0.25f * prog);
            vec4 lc = p.locked ? vec4(1.f, 0.2f, 0.1f, 1.f) : vec4(1.f, 0.8f, 0.2f, 0.9f);
            ui.brackets(cx - bw, cy - bw, bw * 2, bw * 2, bw * 0.4f, 2.f, lc);
            ui.text(cx, cy + bw + 8.f, p.locked ? "LOCKED" : "TRACKING", s * 0.8f, lc, Align::Center);
        } else {
            ui.text(cx, cy + r * 0.55f, "NO LOCK", s * 0.7f, vec4(c.r, c.g, c.b, 0.7f), Align::Center);
        }
    } else {
        // binoculars / thermal: mil scale
        float L = std::min(x1 - x0, y1 - y0) * 0.45f;
        ui.line(cx - L, cy, cx - L * 0.08f, cy, 1.5f, c);
        ui.line(cx + L * 0.08f, cy, cx + L, cy, 1.5f, c);
        ui.line(cx, cy - L, cx, cy - L * 0.08f, 1.5f, c);
        ui.line(cx, cy + L * 0.08f, cx, cy + L, 1.5f, c);
        for (int i = 1; i <= 8; ++i) {
            float d = i * L / 9.f;
            float t = (i % 5 == 0) ? 7.f * s : 3.5f * s;
            ui.line(cx - t, cy + d, cx + t, cy + d, 1.f, c);
            ui.line(cx - t, cy - d, cx + t, cy - d, 1.f, c);
            ui.line(cx + d, cy - t, cx + d, cy + t, 1.f, c);
            ui.line(cx - d, cy - t, cx - d, cy + t, 1.f, c);
        }
    }
    // readouts
    char buf[96];
    vec4 tc = thermal ? vec4(1.f, 0.9f, 0.7f, 0.95f) : (m.env().nightness > 0.5f ? vec4(0.5f, 1.f, 0.5f, 0.9f) : vec4(1.f, 1.f, 1.f, 0.85f));
    float ts = std::max(1.3f, s * 0.75f);
    std::snprintf(buf, sizeof(buf), "%.1fX", dev.zoom);
    ui.text(x0 + 16.f, y1 - 18.f - 8.f * ts, buf, ts, tc);
    std::snprintf(buf, sizeof(buf), "BRG %s  ELV %+02d", bearingStr(m.aimYaw()).c_str(), static_cast<int>(std::lround(degrees(m.aimPitch()))));
    ui.text(x1 - 16.f, y1 - 18.f - 8.f * ts, buf, ts, tc, Align::Right);
    if (thermal) ui.text(x0 + 16.f, y0 + 14.f, m.scene().thermal == SensorState::Degraded ? "THERMAL  LOW-RES" : "THERMAL  IRONBOW", ts, tc);
    else if (dev.item == Item::Binoculars)
        ui.text(x0 + 16.f, y0 + 14.f, m.env().nightness > 0.5f ? (m.scene().binoculars == SensorState::Degraded ? "NV  GAIN LOW" : "NIGHT VISION") : "OPTICS", ts, tc);
    if (w.player.breath < 0.999f) {
        float bw = 90.f * ts * 0.6f;
        ui.rect(x1 - 16.f - bw, y0 + 14.f, bw, 5.f, vec4(0.f, 0.f, 0.f, 0.5f));
        ui.rect(x1 - 16.f - bw, y0 + 14.f, bw * w.player.breath, 5.f, tc);
    }
    // markers on tagged drones
    if (w.threatTime >= 0.f) {
        for (const auto& d : w.drones) {
            if (!d.alive() || !d.tagged) continue;
            vec2 p;
            if (!project(dev.viewProj, d.pos, dev.hole, p)) continue;
            if (p.x < x0 || p.x > x1 || p.y < y0 || p.y > y1) continue;
            float dist = glm::length(d.pos - dev.camPos);
            if (std::exp(-m.env().fogDensity * (thermal ? 0.22f : 1.f) * dist) < 0.05f) continue;
            float bs = 14.f;
            ui.brackets(p.x - bs, p.y - bs, bs * 2, bs * 2, 5.f, 1.5f, vec4(1.f, 0.3f, 0.15f, 0.9f));
            std::snprintf(buf, sizeof(buf), "%dM", static_cast<int>(dist));
            ui.text(p.x + bs + 3.f, p.y - 3.f, buf, 1.5f, vec4(1.f, 0.4f, 0.2f, 0.9f));
        }
    }
    (void)time;
}

void Hud::acousticTablet(Ui& ui, const Mission& m, const DeviceLayout& dev, float time) {
    const World& w = m.w;
    float x0 = dev.hole.x, y0 = dev.hole.y, x1 = dev.hole.z, y1 = dev.hole.w;
    float W = x1 - x0, H = y1 - y0;
    ui.rect(x0, y0, W, H, vec4(0.01f, 0.035f, 0.02f, 1.f));
    float s = std::max(1.2f, std::floor(H / 330.f));
    vec4 g = kGreen;
    ui.text(x0 + 14, y0 + 10, "ACOUSTIC ARRAY", s * 1.2f, g);
    ui.text(x1 - 14, y0 + 10, m.itemStatus(Item::Acoustic), s * 0.9f, kAmber, Align::Right);
    // polar plot
    float R = std::min(W * 0.3f, H * 0.38f);
    float cx = x0 + W * 0.28f, cy = y0 + H * 0.54f;
    for (int i = 1; i <= 4; ++i) ui.ring(cx, cy, R * i / 4.f - 0.8f, R * i / 4.f, 0, kTwoPi, vec4(0.2f, 0.5f, 0.3f, 0.6f), 80);
    for (int i = 0; i < 12; ++i) {
        float a = i * kTwoPi / 12.f;
        ui.line(cx, cy, cx + std::sin(a) * R, cy - std::cos(a) * R, 1.f, vec4(0.2f, 0.5f, 0.3f, 0.35f));
    }
    ui.text(cx, cy - R - 14, "N", s, kAmber, Align::Center);
    ui.text(cx + R + 8, cy - 4, "E", s, kDim);
    ui.text(cx, cy + R + 6, "S", s, kDim);
    ui.text(cx - R - 14, cy - 4, "W", s, kDim);
    // aim wedge
    float ya = m.aimYaw();
    float half = radians(m.scene().audio == AudioLevel::High ? 7.f : (m.scene().audio == AudioLevel::Medium ? 11.f : 17.f));
    ui.ring(cx, cy, 0.f, R, ya - half, ya + half, vec4(0.3f, 0.9f, 0.4f, 0.15f), 24);
    ui.line(cx, cy, cx + std::sin(ya) * R, cy - std::cos(ya) * R, 2.f, kAmber);
    // contacts
    for (const auto& c : w.acoustic) {
        float a = c.bearing;
        float rr = R * (0.25f + 0.7f * (1.f - glm::clamp(c.range / 1800.f, 0.f, 1.f)));
        float width = radians(m.scene().audio == AudioLevel::High ? 3.f : (m.scene().audio == AudioLevel::Medium ? 6.f : 11.f));
        float pulse = 0.6f + 0.4f * std::sin(time * 9.f + a * 5.f);
        ui.ring(cx, cy, std::max(0.f, rr - 8 * s), rr + 8 * s, a - width, a + width, vec4(1.f, 0.6f * (1.f - c.strength) + 0.3f, 0.2f, 0.25f + 0.6f * c.strength * pulse), 16);
    }
    if (w.acousticJam > 0.f) {
        ui.text(cx, cy, "SIGNAL SATURATED", s, kRed, Align::Center);
    }
    // waveform
    float wx = x0 + W * 0.56f, wy = y0 + H * 0.2f, ww = W * 0.4f, wh = H * 0.34f;
    ui.rect(wx, wy, ww, wh, vec4(0.0f, 0.02f, 0.01f, 1.f));
    ui.rectOutline(wx, wy, ww, wh, 1.f, vec4(0.2f, 0.5f, 0.3f, 0.8f));
    std::vector<vec2> pts;
    int n = static_cast<int>(w.audioWave.size());
    for (int i = 0; i < n; ++i) {
        float a = w.audioWave[static_cast<size_t>(i)];
        float sgn = (i & 1) ? 1.f : -1.f;
        pts.push_back(vec2(wx + ww * i / std::max(1, n - 1), wy + wh * 0.5f + sgn * a * wh * 0.45f));
    }
    ui.polyline(pts, 1.5f, g);
    ui.text(wx, wy - 12 * s * 0.7f, "AUDIO ENVELOPE", s * 0.7f, kDim);
    // readouts
    char buf[96];
    float best = 0.f;
    for (const auto& c : w.acoustic) best = std::max(best, c.strength);
    std::snprintf(buf, sizeof(buf), "LEVEL  %3d%%", static_cast<int>(best * 100.f));
    ui.text(wx, wy + wh + 14, buf, s, g);
    ui.rect(wx, wy + wh + 14 + 12 * s, ww, 8, vec4(0, 0, 0, 0.5f));
    ui.rect(wx, wy + wh + 14 + 12 * s, ww * best, 8, best > 0.6f ? kRed : g);
    std::snprintf(buf, sizeof(buf), "AIM  %s", bearingStr(ya).c_str());
    ui.text(wx, wy + wh + 14 + 26 * s, buf, s, kAmber);
    ui.text(wx, y1 - 22 - 20 * s, "SPACE: TAG BEARING IN WEDGE", s * 0.75f, kDim);
}

void Hud::rfTablet(Ui& ui, const Mission& m, const DeviceLayout& dev, float time) {
    const World& w = m.w;
    float x0 = dev.hole.x, y0 = dev.hole.y, x1 = dev.hole.z, y1 = dev.hole.w;
    float W = x1 - x0, H = y1 - y0;
    ui.rect(x0, y0, W, H, vec4(0.01f, 0.02f, 0.035f, 1.f));
    float s = std::max(1.2f, std::floor(H / 330.f));
    vec4 c = vec4(0.4f, 0.8f, 1.f, 0.95f);
    ui.text(x0 + 14, y0 + 10, "RF SPECTRUM SCANNER", s * 1.2f, c);
    ui.text(x1 - 14, y0 + 10, m.itemStatus(Item::Rf), s * 0.9f, kAmber, Align::Right);
    if (m.scene().signal == SensorState::Unavailable) {
        ui.text((x0 + x1) * 0.5f, (y0 + y1) * 0.5f, "NO CARRIER", s * 2.f, kRed, Align::Center);
        return;
    }
    // waterfall
    float wx = x0 + 18, wy = y0 + 14 + 14 * s * 1.2f, ww = W * 0.62f, wh = H * 0.62f;
    int rows = static_cast<int>(w.waterfall.size());
    float cw = ww / 128.f, rh = wh / 72.f;
    ui.rect(wx - 2, wy - 2, ww + 4, wh + 4, vec4(0, 0, 0, 1));
    for (int r = 0; r < rows; ++r) {
        const auto& row = w.waterfall[static_cast<size_t>(rows - 1 - r)];
        for (int i = 0; i < 128; ++i) {
            float v = row[static_cast<size_t>(i)] / 255.f;
            if (v < 0.12f) continue;
            vec4 col = v < 0.5f ? vec4(0.f, v * 1.2f, v * 2.4f, 1.f) : vec4((v - 0.5f) * 2.f, 0.6f + (v - 0.5f), 1.f - (v - 0.5f), 1.f);
            ui.rect(wx + i * cw, wy + r * rh, cw + 0.5f, rh + 0.5f, col);
        }
    }
    ui.rectOutline(wx - 2, wy - 2, ww + 4, wh + 4, 1.f, vec4(0.3f, 0.5f, 0.7f, 0.9f));
    char buf[96];
    for (int i = 0; i <= 4; ++i) {
        float x = wx + ww * i / 4.f;
        ui.line(x, wy + wh + 2, x, wy + wh + 8, 1.f, c);
        std::snprintf(buf, sizeof(buf), "%.3f", 2.400f + 0.025f * i);
        ui.text(x, wy + wh + 12, buf, s * 0.6f, kDim, Align::Center);
    }
    ui.text(wx + ww * 0.5f, wy + wh + 12 + 10 * s * 0.6f, "GHZ", s * 0.6f, kDim, Align::Center);
    // contact list
    float lx = wx + ww + 24, ly = wy;
    ui.text(lx, ly, "LINKS", s, c);
    ly += 12 * s;
    int k = 0;
    for (const auto& ct : w.rf) {
        if (k++ > 6) break;
        std::snprintf(buf, sizeof(buf), "%.3fG  BRG %s", ct.freq, bearingStr(ct.bearing).c_str());
        ui.text(lx, ly, buf, s * 0.7f, vec4(1.f, 0.85f, 0.3f, 0.5f + 0.5f * ct.strength));
        ly += 9 * s;
        ui.rect(lx, ly, 100.f * ct.strength, 4.f, kGreen);
        ly += 8 * s;
    }
    if (w.rf.empty()) ui.text(lx, ly, "NO LINKS", s * 0.7f, kDim);
    if (w.rfJam > 0.f) ui.text(wx + ww * 0.5f, wy + wh * 0.5f, "JAMMED", s * 2.f, vec4(1.f, 0.2f, 0.1f, 0.6f + 0.4f * std::sin(time * 12.f)), Align::Center);
    std::snprintf(buf, sizeof(buf), "AIM  %s", bearingStr(m.aimYaw()).c_str());
    ui.text(lx, y1 - 60, buf, s, kAmber);
    ui.text(lx, y1 - 40, "SPACE: TAG LINK", s * 0.7f, kDim);
}

void Hud::briefing(Ui& ui, const Mission& m, float time) {
    float W = static_cast<float>(ui.width()), H = static_cast<float>(ui.height());
    ui.rect(0, 0, W, H, vec4(0.01f, 0.02f, 0.015f, 0.82f));
    float s = std::max(2.f, std::floor(H / 300.f));
    float px = W * 0.5f - std::min(W * 0.4f, 380.f * s * 0.55f);
    float y = H * 0.14f;
    ui.text(W * 0.5f, y - 30, "FITWING  //  OBSERVATION POST BRIEFING", s * 1.25f, kAmber, Align::Center);
    ui.line(px, y - 6, W - px, y - 6, 2.f, kDim);
    // body: split lines
    std::string line;
    for (char ch : m.w.briefing + "\n") {
        if (ch == '\n') {
            ui.text(px, y, line, s * 0.85f, line.rfind("INTEL", 0) == 0 || line.rfind("MISSION", 0) == 0 ? kAmber : kGreen);
            y += 11.f * s * 0.85f;
            line.clear();
        } else {
            line += ch;
        }
    }
    y = H - H * 0.16f;
    float a = 0.55f + 0.45f * std::sin(time * 3.f);
    ui.text(W * 0.5f, y, "CLICK OR PRESS ENTER TO START YOUR SHIFT", s, vec4(kGreen.r, kGreen.g, kGreen.b, a), Align::Center);
    ui.text(W * 0.5f, y + 14.f * s, "ESC: ABORT   RMB: NEXT ITEM   LMB: RAISE / LOWER   SPACE: ACTION   1-2-3: CLASSIFY", s * 0.6f, kDim, Align::Center);
}

void Hud::outcome(Ui& ui, const Mission& m, float time) {
    const World& w = m.w;
    float W = static_cast<float>(ui.width()), H = static_cast<float>(ui.height());
    float a = glm::clamp((w.endTimer - (w.jumpscare ? 1.8f : 0.f)) / 0.8f, 0.f, 1.f);
    ui.rect(0, 0, W, H, vec4(0.f, 0.f, 0.f, 0.75f * a));
    bool ok = w.phase == Phase::Success;
    vec4 col = ok ? vec4(0.5f, 1.f, 0.55f, a) : vec4(1.f, 0.25f, 0.15f, a);
    float s = std::max(2.f, std::floor(H / 260.f));
    ui.text(W * 0.5f, H * 0.28f, w.outcomeTitle, s * 1.6f, col, Align::Center);
    ui.text(W * 0.5f, H * 0.28f + 20.f * s, w.outcomeSub, s * 0.7f, vec4(0.8f, 0.8f, 0.8f, a), Align::Center);
    const MissionResult& r = m.result();
    char buf[96];
    float y = H * 0.50f;
    auto row = [&](const char* k, const char* v) {
        ui.text(W * 0.5f - 20, y, k, s * 0.7f, vec4(0.6f, 0.8f, 0.6f, a), Align::Right);
        ui.text(W * 0.5f + 20, y, v, s * 0.7f, vec4(1.f, 1.f, 1.f, a));
        y += 11.f * s * 0.75f;
    };
    std::snprintf(buf, sizeof(buf), "%.1f S", r.detectionTime); row("DETECTION", buf);
    std::snprintf(buf, sizeof(buf), "%.1f S", r.classificationTime); row("CLASSIFICATION", buf);
    std::snprintf(buf, sizeof(buf), "%.1f S", r.responseTime); row("RESPONSE", buf);
    std::snprintf(buf, sizeof(buf), "%d / %d", r.shotsFired, r.missilesFired); row("SHOTS / MISSILES", buf);
    std::snprintf(buf, sizeof(buf), "%.0f", r.overallScore); row("OVERALL SCORE", buf);
    float t = w.endTimer;
    if (t > 2.f && std::fmod(time, 1.4f) < 0.9f)
        ui.text(W * 0.5f, H * 0.86f, "CLICK TO CLOSE AND SEND REPORT", s * 0.8f, kGreen, Align::Center);
}

void Hud::jumpscare(Ui& ui, const Mission& m, float time) {
    float W = static_cast<float>(ui.width()), H = static_cast<float>(ui.height());
    float t = m.w.endTimer;
    float k = glm::clamp(t / 0.25f, 0.f, 1.f);
    float shake = (1.f - glm::clamp(t / 1.8f, 0.f, 1.f)) * 22.f;
    float jx = (hash01(static_cast<uint32_t>(time * 60.f)) - 0.5f) * shake;
    float jy = (hash01(static_cast<uint32_t>(time * 60.f) + 77u) - 0.5f) * shake;
    ui.rect(0, 0, W, H, vec4(0.05f, 0.0f, 0.0f, 0.85f * k));
    float cx = W * 0.5f + jx, cy = H * 0.5f + jy;
    float R = H * (0.20f + 0.55f * k);
    // rotor discs
    for (int i = 0; i < 4; ++i) {
        float a = i * kPi * 0.5f + kPi * 0.25f;
        ui.circle(cx + std::cos(a) * R * 1.5f, cy + std::sin(a) * R * 1.1f, R * 0.7f, vec4(0.02f, 0.02f, 0.02f, 0.85f), 36);
        ui.ring(cx + std::cos(a) * R * 1.5f, cy + std::sin(a) * R * 1.1f, R * 0.66f, R * 0.7f, 0, kTwoPi, vec4(0.5f, 0.5f, 0.5f, 0.6f), 36);
        ui.line(cx, cy, cx + std::cos(a) * R * 1.5f, cy + std::sin(a) * R * 1.1f, R * 0.07f, vec4(0.07f, 0.07f, 0.07f, 1.f));
    }
    ui.circle(cx, cy, R, vec4(0.09f, 0.1f, 0.09f, 1.f), 48);
    ui.ring(cx, cy, R * 0.93f, R, 0, kTwoPi, vec4(0.25f, 0.27f, 0.25f, 1.f), 48);
    ui.circle(cx, cy, R * 0.42f, vec4(0.f, 0.f, 0.f, 1.f), 40);
    ui.ring(cx, cy, R * 0.36f, R * 0.42f, 0, kTwoPi, vec4(0.3f, 0.3f, 0.35f, 1.f), 40);
    ui.circle(cx, cy, R * 0.16f, vec4(0.9f, 0.05f, 0.03f, 1.f), 24);
    ui.circle(cx - R * 0.07f, cy - R * 0.08f, R * 0.04f, vec4(1.f, 0.9f, 0.9f, 0.9f), 10);
    ui.circle(cx - R * 0.6f, cy + R * 0.55f, R * 0.06f, vec4(1.f, 0.05f, 0.02f, 1.f), 12);
    ui.circle(cx + R * 0.6f, cy + R * 0.55f, R * 0.06f, vec4(0.1f, 1.f, 0.1f, 1.f), 12);
    // static
    for (int i = 0; i < 90; ++i) {
        float y = hash01(static_cast<uint32_t>(i) * 97u + static_cast<uint32_t>(time * 30.f) * 13u) * H;
        float h = 1.f + hash01(static_cast<uint32_t>(i) * 31u + 5u) * 6.f;
        ui.rect(0, y, W, h, vec4(1.f, 1.f, 1.f, 0.07f * (1.f - t / 1.8f)));
    }
}

}  // namespace aegis
