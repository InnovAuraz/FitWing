#include "gfx/renderer.h"

#include <cstring>

#include "core/paths.h"

namespace aegis {

namespace {
float ease(float t) { return t * t * (3.f - 2.f * t); }

mat4 basisFromFwd(const vec3& fwd, float roll = 0.f) {
    vec3 f = glm::normalize(fwd);
    vec3 r = glm::cross(f, vec3(0, 1, 0));
    if (glm::length(r) < 1e-4f) r = vec3(1, 0, 0);
    r = glm::normalize(r);
    vec3 u = glm::cross(r, f);
    mat4 m(1.0f);
    m[0] = vec4(r, 0);
    m[1] = vec4(u, 0);
    m[2] = vec4(-f, 0);
    if (roll != 0.f) m = m * glm::rotate(mat4(1.0f), roll, vec3(0, 0, 1));
    return m;
}

struct Frustum {
    vec4 p[6];
    explicit Frustum(const mat4& m) {
        for (int i = 0; i < 3; ++i)
            for (int s = 0; s < 2; ++s) {
                float sg = s == 0 ? 1.f : -1.f;
                vec4 row(m[0][3] + sg * m[0][i], m[1][3] + sg * m[1][i], m[2][3] + sg * m[2][i], m[3][3] + sg * m[3][i]);
                float l = glm::length(vec3(row));
                p[i * 2 + s] = row / l;
            }
    }
    bool sphere(const vec3& c, float r) const {
        for (const auto& pl : p)
            if (glm::dot(vec3(pl), c) + pl.w < -r) return false;
        return true;
    }
};
}  // namespace

// ---------------------------------------------------------------------------------------------
bool Renderer::init(int w, int h, const SceneDef& scene, const Environment& env, Terrain& terrain, AssetCatalog& assets, int msaa) {
    scene_ = &scene;
    env_ = &env;
    terrain_ = &terrain;
    assets_ = &assets;
    msaa_ = msaa;
    bool ok = true;
    ok &= sky_.load("sky");
    ok &= terrainSh_.load("terrain");
    ok &= water_.load("water");
    ok &= obj_.load("object");
    ok &= objInst_.load("object", {"INSTANCED"});
    ok &= shadowSh_.load("shadow");
    ok &= shadowInst_.load("shadow", {"INSTANCED"});
    ok &= particle_.load("particle");
    ok &= rain_.load("rain");
    ok &= line_.load("line");
    ok &= bloomDown_.load("bloom", {"DOWN"});
    ok &= bloomBlur_.load("bloom");
    ok &= composite_.load("composite");
    ok &= device_.load("device");
    ok &= frame_.load("frame");
    if (!ok) {
        LOG_ERROR("shader compilation failed");
        return false;
    }
    if (!ui_.init()) return false;

    glGenVertexArrays(1, &emptyVao_);
    glGenBuffers(1, &instVbo_);
    glGenVertexArrays(1, &partVao_);
    glGenBuffers(1, &partVbo_);
    glBindVertexArray(partVao_);
    glBindBuffer(GL_ARRAY_BUFFER, partVbo_);
    for (int i = 0; i < 3; ++i) {
        glEnableVertexAttribArray(i);
        glVertexAttribPointer(i, 4, GL_FLOAT, GL_FALSE, sizeof(float) * 12, reinterpret_cast<void*>(sizeof(float) * 4 * i));
        glVertexAttribDivisor(i, 1);
    }
    glBindVertexArray(0);
    glGenVertexArrays(1, &lineVao_);
    glGenBuffers(1, &lineVbo_);
    glBindVertexArray(lineVao_);
    glBindBuffer(GL_ARRAY_BUFFER, lineVbo_);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 7, nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(float) * 7, reinterpret_cast<void*>(sizeof(float) * 3));
    glBindVertexArray(0);

    // wires
    glGenVertexArrays(1, &wireVao_);
    glGenBuffers(1, &wireVbo_);
    std::vector<float> wd;
    for (const auto& p : terrain_->wireLines) {
        wd.insert(wd.end(), {p.x, p.y, p.z, 0.03f, 0.03f, 0.035f, 0.9f});
    }
    wireCount_ = static_cast<int>(terrain_->wireLines.size());
    glBindVertexArray(wireVao_);
    glBindBuffer(GL_ARRAY_BUFFER, wireVbo_);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(wd.size() * sizeof(float)), wd.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 7, nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(float) * 7, reinterpret_cast<void*>(sizeof(float) * 3));
    glBindVertexArray(0);

    unsigned char orm[4] = {255, 204, 0, 255};
    defaultOrm_ = createTexture2D(1, 1, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, orm, false, GL_NEAREST, GL_NEAREST, GL_REPEAT);

    terrain_->buildMeshes(terrainMesh_, waterMesh_);
    cab_ = buildCab();
    if (!shadow_.create(2048)) LOG_WARN("shadow map unavailable");
    resize(w, h);
    return true;
}

void Renderer::resize(int w, int h) {
    W_ = std::max(16, w);
    H_ = std::max(16, h);
    scene_msaa_.create(W_, H_, GL_RGBA16F, msaa_);
    scene_res_.create(W_, H_, GL_RGBA16F, 0);
    bloomA_.create(std::max(8, W_ / 2), std::max(8, H_ / 2), GL_RGBA16F, 0);
    bloomB_.create(std::max(8, W_ / 4), std::max(8, H_ / 4), GL_RGBA16F, 0);
    bloomC_.create(std::max(8, W_ / 4), std::max(8, H_ / 4), GL_RGBA16F, 0);
    zoom_msaa_.destroy();
    zoom_res_.destroy();
}

void Renderer::shutdown() {
    ui_.shutdown();
    scene_msaa_.destroy();
    scene_res_.destroy();
    zoom_msaa_.destroy();
    zoom_res_.destroy();
    bloomA_.destroy();
    bloomB_.destroy();
    bloomC_.destroy();
    shadow_.destroy();
}

bool Renderer::ensureZoomTarget(int w, int h) {
    w = std::max(16, w);
    h = std::max(16, h);
    if (zoom_res_.fbo && zoom_res_.w == w && zoom_res_.h == h) return true;
    zoom_msaa_.create(w, h, GL_RGBA16F, msaa_);
    zoom_res_.create(w, h, GL_RGBA16F, 0);
    return zoom_res_.fbo != 0;
}

void Renderer::fullscreen() const {
    glBindVertexArray(emptyVao_);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindVertexArray(0);
}

void Renderer::resolve(const RenderTarget& src, const RenderTarget& dst) const {
    glBindFramebuffer(GL_READ_FRAMEBUFFER, src.fbo);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, dst.fbo);
    glBlitFramebuffer(0, 0, src.w, src.h, 0, 0, dst.w, dst.h, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

Camera Renderer::makeCamera(const vec3& pos, const vec3& dir, float fovY, int w, int h, float nearZ) const {
    Camera c;
    c.pos = pos;
    c.fwd = glm::normalize(dir);
    c.right = glm::normalize(glm::cross(c.fwd, vec3(0, 1, 0)));
    c.up = glm::cross(c.right, c.fwd);
    c.fovY = fovY;
    c.w = w;
    c.h = h;
    c.aspect = static_cast<float>(w) / static_cast<float>(h);
    c.nearZ = nearZ;
    c.view = glm::lookAt(pos, pos + c.fwd, c.up);
    c.proj = glm::perspective(fovY, c.aspect, nearZ, c.farZ);
    c.vp = c.proj * c.view;
    return c;
}

// ---------------------------------------------------------------------------------------------
DeviceLayout Renderer::layout(const Mission& m) const {
    DeviceLayout d;
    const Player& p = m.w.player;
    float W = static_cast<float>(W_), H = static_cast<float>(H_);
    if (p.raise < 0.01f || m.w.phase == Phase::Briefing) return d;
    d.active = true;
    d.item = p.item;
    float off = (1.f - ease(p.raise)) * H * 1.08f;
    d.offsetY = off;
    d.zoom = p.zoom[static_cast<int>(p.item)];
    d.fovY = itemFovY(p.item, d.zoom);
    d.hasImage = true;
    switch (p.item) {
        case Item::Binoculars:
            d.style = 0;
            d.outer = vec4(0.f, 0.f, W, H);
            d.hole = vec4(0.10f * W, 0.11f * H, 0.90f * W, 0.79f * H);
            d.cornerOuter = 0.f;
            d.cornerHole = 0.05f * H;
            d.blackout = 1.f;
            d.hasImage = m.itemUsable(Item::Binoculars);
            break;
        case Item::Thermal:
            d.style = 1;
            d.outer = vec4(0.07f * W, 0.05f * H, 0.93f * W, 0.97f * H);
            d.hole = vec4(0.13f * W, 0.11f * H, 0.87f * W, 0.80f * H);
            d.cornerOuter = 0.06f * H;
            d.cornerHole = 0.03f * H;
            d.blackout = 0.f;
            d.hasImage = m.itemUsable(Item::Thermal);
            break;
        case Item::Acoustic:
        case Item::Rf:
            d.style = 2;
            d.outer = vec4(0.11f * W, 0.07f * H, 0.89f * W, 0.95f * H);
            d.hole = vec4(0.155f * W, 0.13f * H, 0.845f * W, 0.84f * H);
            d.cornerOuter = 0.04f * H;
            d.cornerHole = 0.015f * H;
            d.blackout = 0.f;
            d.hasImage = false;
            break;
        case Item::Rifle:
        case Item::Launcher: {
            d.style = 3;
            float cx = W * 0.5f, cy = H * 0.5f;
            float so = 0.40f * H, sh = 0.32f * H;
            d.outer = vec4(cx - so, cy - so, cx + so, cy + so);
            d.hole = vec4(cx - sh, cy - sh, cx + sh, cy + sh);
            d.cornerOuter = so;
            d.cornerHole = sh;
            d.circle = true;
            d.blackout = 0.f;
            break;
        }
        default: d.active = false; return d;
    }
    d.outer.y += off;
    d.outer.w += off;
    d.hole.y += off;
    d.hole.w += off;
    return d;
}

// ---------------------------------------------------------------------------------------------
void Renderer::gatherLights(const Mission& m) {
    numLights_ = 0;
    const World& w = m.w;
    const Environment& e = *env_;
    float base = e.sunElev > 0.25f ? 0.f : glm::mix(0.35f, 1.0f, e.nightness);
    lampLevel_ = base * (1.f - w.flicker);
    if (lampLevel_ > 0.01f) lights_[numLights_++] = {cabLampPos() - vec3(0, 0.15f, 0), vec3(10.f, 7.2f, 4.2f) * lampLevel_, 14.f};
    {   // soft bounce light inside the cab (proportional to the sky ambient)
        vec3 fill = e.ambSky * 7.f + e.ambGround * 2.f;
        if (glm::dot(fill, vec3(0.333f)) > 0.02f) lights_[numLights_++] = {vec3(0.f, 11.9f, 0.f), fill, 6.5f};
    }
    for (const auto& ms : w.missiles) {
        if (!ms.alive || numLights_ >= 3) continue;
        lights_[numLights_++] = {ms.pos, vec3(900.f, 500.f, 200.f), 90.f};
        break;
    }
    float bestScore = 0.f;
    const Particle* best = nullptr;
    for (const auto& p : w.particles) {
        if (p.type != 1 || p.size < 1.2f) continue;
        float lf = p.life / p.maxLife;
        float sc = p.size * p.color.a * lf / (glm::length(p.pos) + 20.f);
        if (sc > bestScore) { bestScore = sc; best = &p; }
    }
    if (best && numLights_ < 4) {
        float lf = best->life / best->maxLife;
        lights_[numLights_++] = {best->pos, vec3(1500.f, 750.f, 260.f) * lf * best->color.a, 130.f};
    }
}

void Renderer::setCommon(const Shader& s, const Camera& c, bool thermal, const Mission& m) {
    const Environment& e = *env_;
    s.use();
    s.set("u_viewProj", c.vp);
    s.set("u_camPos", c.pos);
    s.set("u_keyDir", e.keyDir);
    s.set("u_keyColor", e.keyColor);
    s.set("u_ambSky", e.ambSky);
    s.set("u_ambGround", e.ambGround);
    s.set("u_fogColor", e.fogColor);
    s.set("u_fogDensity", e.fogDensity);
    s.set("u_wet", e.wet);
    s.set("u_flash", m.w.flash);
    s.set("u_time", m.w.time);
    s.set("u_thermal", thermal ? 1.f : 0.f);
    s.set("u_thermalFog", 0.22f);
    s.set("u_heatAmbient", 0.02f + 0.06f * e.dayWarm);
    s.set("u_emissiveBoost", 1.f);
    s.set("u_numLights", numLights_);
    vec3 pos[4], col[4];
    float rng[4];
    for (int i = 0; i < 4; ++i) {
        pos[i] = i < numLights_ ? lights_[i].pos : vec3(0.f);
        col[i] = i < numLights_ ? lights_[i].col : vec3(0.f);
        rng[i] = i < numLights_ ? lights_[i].range : 1.f;
    }
    glUniform3fv(s.loc("u_lightPos"), 4, &pos[0].x);
    glUniform3fv(s.loc("u_lightCol"), 4, &col[0].x);
    glUniform1fv(s.loc("u_lightRange"), 4, rng);
    s.set("u_shadowVP", shadowVP_);
    s.set("u_shadowOn", shadowOn_ ? 1.f : 0.f);
    s.set("u_shadowMap", 12);
}

void Renderer::bindMaterial(const Shader& sh, const Material& mt, float heat) {
    auto& lib = TextureLibrary::get();
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, mt.albedo ? mt.albedo : lib.white());
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, mt.normal ? mt.normal : lib.flatNormal());
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, mt.orm ? mt.orm : defaultOrm_);
    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, mt.emissive ? mt.emissive : lib.black());
    sh.set("u_albedoTex", 0);
    sh.set("u_normalTex", 1);
    sh.set("u_ormTex", 2);
    sh.set("u_emissiveTex", 3);
    sh.set("u_baseColor", mt.baseColor);
    sh.set("u_metallic", mt.metallic);
    sh.set("u_roughness", mt.roughness);
    sh.set("u_normalStrength", mt.normalStrength);
    sh.set("u_aoFromOrm", mt.aoFromOrm ? 1.f : 0.f);
    sh.set("u_procedural", mt.procedural ? 1.f : 0.f);
    sh.set("u_emissiveFactor", mt.emissiveFactor);
    sh.set("u_alphaMask", mt.alphaMask ? 1.f : 0.f);
    sh.set("u_alphaCutoff", mt.alphaCutoff);
    sh.set("u_heat", heat);
    if (mt.doubleSided) glDisable(GL_CULL_FACE);
    else glEnable(GL_CULL_FACE);
}

void Renderer::drawModel(const Shader& sh, const Model& model, const mat4& root, bool thermal, const Mission& m, float time, bool shadowOnly,
                         float rotorPhase, bool isCab) {
    (void)thermal;
    const World& w = m.w;
    for (const auto& part : model.parts) {
        const Material& mt = model.mats[static_cast<size_t>(part.material)];
        mat4 local = part.local;
        if (part.rotor != 0) local = local * glm::rotate(mat4(1.0f), rotorPhase * static_cast<float>(part.rotor), vec3(0, 1, 0));
        if (shadowOnly) {
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, mt.albedo ? mt.albedo : TextureLibrary::get().white());
            sh.set("u_albedoTex", 0);
            sh.set("u_alphaMask", mt.alphaMask ? 1.f : 0.f);
            sh.set("u_alphaCutoff", mt.alphaCutoff);
            sh.set("u_alphaBase", mt.baseColor.a);
            sh.set("u_model", root * local);
            part.mesh.draw();
            continue;
        }
        bindMaterial(sh, mt, part.heat);
        float boost = 1.f;
        if (isCab) {
            const std::string& n = part.name;
            if (n == "lamp_bulb") boost = std::max(0.08f, lampLevel_);
            else if (n == "alarm_lamp") boost = 0.04f + w.alarm * (0.6f + 0.4f * std::sin(time * 11.f));
            else if (n.rfind("monitor_screen", 0) == 0) boost = 0.7f + 0.3f * hash01(static_cast<uint32_t>(time * 12.f) + 5u) + 0.25f * w.flicker;
        }
        sh.set("u_emissiveBoost", boost);
        sh.set("u_model", root * local);
        part.mesh.draw();
    }
    glEnable(GL_CULL_FACE);
}

// ---------------------------------------------------------------------------------------------
void Renderer::drawProps(const Camera& c, bool thermal, const Mission& m, bool shadow) {
    const Shader& sh = shadow ? shadowInst_ : objInst_;
    Frustum fr(c.vp);
    float vis = effectiveVisibility(*scene_);
    float cull = glm::clamp(vis * (thermal ? 4.5f : 1.1f), 300.f, 7000.f);
    if (shadow) cull = 170.f;
    float pxAng = c.fovY / static_cast<float>(c.h);
    for (int i = static_cast<int>(AssetId::TreePine); i < static_cast<int>(AssetId::Count); ++i) {
        const auto& list = terrain_->props[i];
        if (list.empty()) continue;
        const Model& model = assets_->get(static_cast<AssetId>(i));
        float mr = model.radius();
        instScratch_.clear();
        for (const auto& pi : list) {
            float r = mr * pi.maxScale;
            vec3 cpos = pi.pos + vec3(0, r * 0.5f, 0);
            float d = glm::length(cpos - c.pos);
            if (d > cull) continue;
            if (!shadow && (2.f * r) / std::max(d, 1.f) < 0.8f * pxAng) continue;
            if (!shadow && !fr.sphere(cpos, r)) continue;
            instScratch_.push_back(pi.m);
        }
        if (instScratch_.empty()) continue;
        glBindBuffer(GL_ARRAY_BUFFER, instVbo_);
        glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(instScratch_.size() * sizeof(mat4)), instScratch_.data(), GL_STREAM_DRAW);
        for (const auto& part : model.parts) {
            const Material& mt = model.mats[static_cast<size_t>(part.material)];
            if (shadow) {
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, mt.albedo ? mt.albedo : TextureLibrary::get().white());
                sh.set("u_albedoTex", 0);
                sh.set("u_alphaMask", mt.alphaMask ? 1.f : 0.f);
                sh.set("u_alphaCutoff", mt.alphaCutoff);
                sh.set("u_alphaBase", mt.baseColor.a);
            } else {
                bindMaterial(sh, mt, part.heat);
            }
            sh.set("u_model", part.local);
            part.mesh.drawInstanced(instVbo_, static_cast<int>(instScratch_.size()));
        }
    }
    glEnable(GL_CULL_FACE);
    (void)m;
}

void Renderer::drawDrones(const Camera& c, bool thermal, const Mission& m, bool shadow) {
    const Shader& sh = shadow ? shadowSh_ : obj_;
    const Model& dm = assets_->get(AssetId::Drone);
    Frustum fr(c.vp);
    float pxAng = c.fovY / static_cast<float>(c.h);
    for (const auto& d : m.w.drones) {
        if (d.state == Drone::Dead) continue;
        float dist = glm::length(d.pos - c.pos);
        float r = dm.radius();
        if (!shadow) {
            if (!fr.sphere(d.pos, r)) continue;
            if ((2.f * r) / std::max(dist, 1.f) < 0.25f * pxAng) continue;
        } else if (glm::length(d.pos) > 150.f) {
            continue;
        }
        mat4 R = basisFromFwd(d.fwd, d.bank);
        // nose-down lean with speed
        float sp = glm::length(d.vel);
        R = R * glm::rotate(mat4(1.0f), -std::min(0.35f, sp * 0.012f), vec3(1, 0, 0));
        if (d.state == Drone::Falling) R = R * glm::rotate(mat4(1.0f), d.fallT * 5.f, glm::normalize(vec3(0.3f, 0.2f, 1.f)));
        mat4 root = glm::translate(mat4(1.0f), d.pos) * R;
        if (d.hitFlash > 0.f && !shadow) {
            sh.use();
        }
        drawModel(sh, dm, root, thermal, m, m.w.time, shadow, d.rotorPhase);
    }
    // missiles
    const Model& mm = assets_->get(AssetId::Missile);
    for (const auto& ms : m.w.missiles) {
        if (!ms.alive) continue;
        if (shadow && glm::length(ms.pos) > 150.f) continue;
        mat4 root = glm::translate(mat4(1.0f), ms.pos) * basisFromFwd(ms.vel);
        drawModel(sh, mm, root, thermal, m, m.w.time, shadow);
    }
}

void Renderer::drawLines(const Camera& c, bool thermal, const Mission& m) {
    const World& w = m.w;
    line_.use();
    line_.set("u_viewProj", c.vp);
    line_.set("u_camPos", c.pos);
    line_.set("u_fogColor", env_->fogColor);
    line_.set("u_fogDensity", env_->fogDensity);
    line_.set("u_thermal", thermal ? 1.f : 0.f);
    line_.set("u_thermalFog", 0.22f);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    line_.set("u_additive", 0.f);
    if (wireCount_ > 0) {
        glBindVertexArray(wireVao_);
        glDrawArrays(GL_LINES, 0, wireCount_);
    }
    lineScratch_.clear();
    auto push = [&](const vec3& a, const vec3& b, const vec4& col) {
        lineScratch_.insert(lineScratch_.end(), {a.x, a.y, a.z, col.r, col.g, col.b, col.a, b.x, b.y, b.z, col.r, col.g, col.b, col.a});
    };
    for (const auto& b : w.birds) {
        vec3 v = b.vel;
        vec3 f = glm::length(v) > 0.01f ? glm::normalize(v) : vec3(1, 0, 0);
        vec3 r = glm::normalize(glm::cross(f, vec3(0, 1, 0)));
        float flap = std::sin(b.phase) * 0.18f;
        vec3 cpos = b.pos;
        vec4 col(0.02f, 0.02f, 0.025f, 0.95f);
        push(cpos - r * 0.28f + vec3(0, flap, 0), cpos, col);
        push(cpos, cpos + r * 0.28f + vec3(0, flap, 0), col);
        push(cpos, cpos - f * 0.2f, col);
    }
    if (!lineScratch_.empty()) {
        glBindVertexArray(lineVao_);
        glBindBuffer(GL_ARRAY_BUFFER, lineVbo_);
        glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(lineScratch_.size() * sizeof(float)), lineScratch_.data(), GL_STREAM_DRAW);
        glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(lineScratch_.size() / 7));
    }
    // tracers (additive)
    lineScratch_.clear();
    for (const auto& t : w.tracers) {
        float a = 1.f - t.age / 0.12f;
        push(t.a, t.b, vec4(1.f, 0.85f, 0.45f, 0.9f * a));
    }
    if (!lineScratch_.empty()) {
        glBlendFunc(GL_SRC_ALPHA, GL_ONE);
        line_.set("u_additive", 1.f);
        glBindVertexArray(lineVao_);
        glBindBuffer(GL_ARRAY_BUFFER, lineVbo_);
        glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(lineScratch_.size() * sizeof(float)), lineScratch_.data(), GL_STREAM_DRAW);
        glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(lineScratch_.size() / 7));
    }
    glBindVertexArray(0);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
}

void Renderer::drawParticles(const Camera& c, bool thermal, const Mission& m) {
    const World& w = m.w;
    struct P { float v[12]; float dist; };
    std::vector<P> normal, addv;
    normal.reserve(w.particles.size());
    auto add = [&](const vec3& pos, float size, const vec4& col, float rot, float type, float heat, float minPx, bool additive) {
        P p;
        float v[12] = {pos.x, pos.y, pos.z, size, col.r, col.g, col.b, col.a, rot, type, heat, minPx};
        std::memcpy(p.v, v, sizeof(v));
        p.dist = glm::length(pos - c.pos);
        (additive ? addv : normal).push_back(p);
    };
    for (const auto& p : w.particles) {
        float lf = glm::clamp(p.life / p.maxLife, 0.f, 1.f);
        float a = p.color.a * (lf < 0.6f ? lf / 0.6f : 1.f);
        if (p.fadeIn > 0.f) a *= glm::clamp((1.f - lf) * p.maxLife / p.fadeIn, 0.f, 1.f);
        float heat = p.heat * (0.4f + 0.6f * lf);
        add(p.pos, p.size, vec4(p.color.r, p.color.g, p.color.b, a), p.rot, static_cast<float>(p.type), heat, p.minPixels, p.additive);
    }
    // drone navigation lights
    for (const auto& d : w.drones) {
        if (!d.alive()) continue;
        float t = w.time * 1.1f + d.ledPhase;
        bool strobe = std::fmod(t, 1.f) < 0.12f;
        vec3 col = strobe ? vec3(1.f, 1.f, 1.f) : ((d.id & 1) ? vec3(1.f, 0.08f, 0.04f) : vec3(0.1f, 1.f, 0.2f));
        float inten = strobe ? 3.f : 1.1f;
        add(d.pos + vec3(0, -0.06f, 0), 0.07f, vec4(col * inten, 1.f), 0.f, 2.f, 0.f, 1.6f, true);
    }
    if (normal.empty() && addv.empty()) return;
    particle_.use();
    particle_.set("u_viewProj", c.vp);
    particle_.set("u_camRight", c.right);
    particle_.set("u_camUp", c.up);
    particle_.set("u_camPos", c.pos);
    particle_.set("u_pixelAngle", c.fovY / static_cast<float>(c.h));
    particle_.set("u_fogDensity", env_->fogDensity);
    particle_.set("u_thermal", thermal ? 1.f : 0.f);
    particle_.set("u_thermalFog", 0.22f);
    particle_.set("u_noise", 4);
    glActiveTexture(GL_TEXTURE4);
    glBindTexture(GL_TEXTURE_2D, TextureLibrary::get().set(TexKind::Noise).albedo);
    glEnable(GL_BLEND);
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);
    auto flush = [&](std::vector<P>& v, bool additive, bool sortFar) {
        if (v.empty()) return;
        if (sortFar) std::sort(v.begin(), v.end(), [](const P& a, const P& b) { return a.dist > b.dist; });
        std::vector<float> data;
        data.reserve(v.size() * 12);
        for (const auto& p : v) data.insert(data.end(), p.v, p.v + 12);
        glBlendFunc(GL_SRC_ALPHA, additive ? GL_ONE : GL_ONE_MINUS_SRC_ALPHA);
        glBindVertexArray(partVao_);
        glBindBuffer(GL_ARRAY_BUFFER, partVbo_);
        glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(data.size() * sizeof(float)), data.data(), GL_STREAM_DRAW);
        glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, static_cast<GLsizei>(v.size()));
        glBindVertexArray(0);
    };
    flush(normal, false, true);
    flush(addv, true, false);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glEnable(GL_CULL_FACE);
}

void Renderer::drawRain(const Camera& c, bool thermal, const Mission& m) {
    const Environment& e = *env_;
    if (e.rain <= 0.01f) return;
    rain_.use();
    rain_.set("u_viewProj", c.vp);
    rain_.set("u_camPos", c.pos);
    rain_.set("u_time", m.w.time);
    rain_.set("u_wind", e.wind);
    rain_.set("u_amount", e.rain);
    rain_.set("u_thermal", thermal ? 1.f : 0.f);
    rain_.set("u_rainColor", e.fogColor * 1.4f + vec3(0.03f) + vec3(0.6f, 0.65f, 0.8f) * m.w.flash);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);
    glBindVertexArray(emptyVao_);
    glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, 4200);
    glBindVertexArray(0);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glEnable(GL_CULL_FACE);
}

void Renderer::drawSky(const Camera& c, bool thermal, const Mission& m) {
    const Environment& e = *env_;
    sky_.use();
    mat4 rot = c.proj * mat4(mat3(c.view));
    sky_.set("u_invViewRotProj", glm::inverse(rot));
    sky_.set("u_zenith", e.zenith);
    sky_.set("u_horizon", e.horizon);
    sky_.set("u_glow", e.glow);
    sky_.set("u_sunDir", e.sunDir);
    sky_.set("u_moonDir", e.moonDir);
    sky_.set("u_overcast", e.overcast);
    sky_.set("u_time", m.w.time);
    sky_.set("u_cloud", e.cloud);
    sky_.set("u_storm", e.storm);
    sky_.set("u_stars", e.stars);
    sky_.set("u_flash", m.w.flash);
    sky_.set("u_fogColor", e.fogColor);
    sky_.set("u_fogDensity", e.fogDensity);
    sky_.set("u_thermal", thermal ? 1.f : 0.f);
    sky_.set("u_keyColor", e.keyColor);
    sky_.set("u_noise", 4);
    glActiveTexture(GL_TEXTURE4);
    glBindTexture(GL_TEXTURE_2D, TextureLibrary::get().set(TexKind::Noise).albedo);
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    fullscreen();
    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);
}

void Renderer::drawViewmodel(const Camera& c, bool thermal, const Mission& m) {
    const Player& p = m.w.player;
    if (thermal || (p.item != Item::Rifle && p.item != Item::Launcher) || p.raise < 0.01f) return;
    float low = 1.f - ease(p.raise);
    bool rifle = p.item == Item::Rifle;
    vec3 off = rifle ? vec3(0.17f, -0.21f - low * 0.55f, -0.50f + low * 0.15f) : vec3(0.22f, -0.24f - low * 0.55f, -0.42f + low * 0.15f);
    off.x += m.w.player.sway.x * 6.f;
    off.y += m.w.player.sway.y * 6.f - m.w.player.recoil * 1.5f;
    off.z += m.w.player.recoil * 3.f;
    vec3 pos = c.pos + c.right * off.x + c.up * off.y + c.fwd * (-off.z);
    mat4 B(1.0f);
    B[0] = vec4(c.right, 0);
    B[1] = vec4(c.up, 0);
    B[2] = vec4(-c.fwd, 0);
    mat4 root = glm::translate(mat4(1.0f), pos) * B * glm::rotate(mat4(1.0f), rifle ? 0.07f : 0.05f, vec3(0, 1, 0)) *
                glm::rotate(mat4(1.0f), low * 0.5f, vec3(1, 0, 0));
    glClear(GL_DEPTH_BUFFER_BIT);
    setCommon(obj_, c, false, m);
    drawModel(obj_, assets_->get(rifle ? AssetId::Rifle : AssetId::Launcher), root, false, m, m.w.time, false);
}

void Renderer::drawScene(const Camera& c, bool thermal, const Mission& m, bool viewmodel) {
    const Environment& e = *env_;
    vec3 clear = thermal ? vec3(0.04f) : e.fogColor;
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glClearColor(clear.r, clear.g, clear.b, 1.f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    auto& lib = TextureLibrary::get();
    glActiveTexture(GL_TEXTURE12);
    glBindTexture(GL_TEXTURE_2D, shadow_.depthTex);
    drawSky(c, thermal, m);

    // terrain
    setCommon(terrainSh_, c, thermal, m);
    struct Bind { const char* a; const char* n; TexKind k; int unit; };
    const Bind binds[5] = {{"u_grassA", "u_grassN", TexKind::Grass, 0}, {"u_dirtA", "u_dirtN", TexKind::Dirt, 2},
                           {"u_rockA", "u_rockN", TexKind::Rock, 4}, {"u_sandA", "u_sandN", TexKind::Sand, 6},
                           {"u_roadA", "u_roadN", TexKind::Asphalt, 8}};
    for (const auto& b : binds) {
        glActiveTexture(GL_TEXTURE0 + b.unit);
        glBindTexture(GL_TEXTURE_2D, lib.set(b.k).albedo);
        glActiveTexture(GL_TEXTURE0 + b.unit + 1);
        glBindTexture(GL_TEXTURE_2D, lib.set(b.k).normal);
        terrainSh_.set(b.a, b.unit);
        terrainSh_.set(b.n, b.unit + 1);
    }
    glActiveTexture(GL_TEXTURE10);
    glBindTexture(GL_TEXTURE_2D, lib.set(TexKind::Noise).albedo);
    terrainSh_.set("u_noise", 10);
    terrainSh_.set("u_terrainType", terrain_->typeIndex());
    terrainSh_.set("u_coastDir", terrain_->coastDir());
    terrainSh_.set("u_seaLevel", terrain_->seaLevel());
    terrainSh_.set("u_hasSea", terrain_->hasSea() ? 1.f : 0.f);
    terrainSh_.set("u_dayWarm", e.dayWarm);
    glDisable(GL_CULL_FACE);
    terrainMesh_.draw();
    glEnable(GL_CULL_FACE);

    if (terrain_->hasSea() && waterMesh_.valid()) {
        setCommon(water_, c, thermal, m);
        water_.set("u_zenith", e.zenith);
        water_.set("u_horizon", e.horizon);
        water_.set("u_glow", e.glow);
        water_.set("u_sunDir", e.sunDir);
        water_.set("u_moonDir", e.moonDir);
        water_.set("u_overcast", e.overcast);
        water_.set("u_waveAmp", e.waveAmp);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, lib.set(TexKind::Water).normal);
        water_.set("u_waterN", 0);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, lib.set(TexKind::Noise).albedo);
        water_.set("u_noise", 1);
        glDisable(GL_CULL_FACE);
        waterMesh_.draw();
        glEnable(GL_CULL_FACE);
    }

    setCommon(objInst_, c, thermal, m);
    drawProps(c, thermal, m, false);
    setCommon(obj_, c, thermal, m);
    drawModel(obj_, *cab_, mat4(1.0f), thermal, m, m.w.time, false, 0.f, true);
    drawDrones(c, thermal, m, false);
    drawLines(c, thermal, m);
    drawParticles(c, thermal, m);
    drawRain(c, thermal, m);
    if (viewmodel) drawViewmodel(c, thermal, m);
}

void Renderer::shadowPass(const Mission& m) {
    shadowOn_ = false;
    const Environment& e = *env_;
    if (!shadow_.fbo || e.keyDir.y < 0.07f || std::getenv("FITWING_NOSHADOW")) return;
    vec3 L = glm::normalize(e.keyDir);
    vec3 up = std::fabs(L.y) > 0.95f ? vec3(1, 0, 0) : vec3(0, 1, 0);
    mat4 lv = glm::lookAt(vec3(0, 8, 0) + L * 200.f, vec3(0, 8, 0), up);
    mat4 lp = glm::ortho(-140.f, 140.f, -140.f, 140.f, 20.f, 400.f);
    shadowVP_ = lp * lv;
    glBindFramebuffer(GL_FRAMEBUFFER, shadow_.fbo);
    glViewport(0, 0, shadow_.size, shadow_.size);
    glClear(GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(2.f, 4.f);
    glDisable(GL_CULL_FACE);
    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
    shadowSh_.use();
    shadowSh_.set("u_viewProj", shadowVP_);
    drawModel(shadowSh_, *cab_, mat4(1.0f), false, m, 0.f, true);
    drawDrones(Camera(), false, m, true);
    shadowInst_.use();
    shadowInst_.set("u_viewProj", shadowVP_);
    Camera lc;
    lc.pos = vec3(0.f);
    lc.vp = shadowVP_;
    lc.fovY = 1.f;
    lc.h = 1000;
    drawProps(lc, false, m, true);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glDisable(GL_POLYGON_OFFSET_FILL);
    glEnable(GL_CULL_FACE);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    shadowOn_ = true;
}

// ---------------------------------------------------------------------------------------------
void Renderer::bloomPass() {
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    bloomA_.bind();
    bloomDown_.use();
    bloomDown_.set("u_tex", 0);
    bloomDown_.set("u_texel", vec2(1.f / scene_res_.w, 1.f / scene_res_.h));
    bloomDown_.set("u_threshold", 1.0f);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, scene_res_.colorTex);
    fullscreen();
    bloomB_.bind();
    bloomDown_.set("u_texel", vec2(1.f / bloomA_.w, 1.f / bloomA_.h));
    bloomDown_.set("u_threshold", 0.0f);
    glBindTexture(GL_TEXTURE_2D, bloomA_.colorTex);
    fullscreen();
    bloomBlur_.use();
    bloomBlur_.set("u_tex", 0);
    bloomBlur_.set("u_texel", vec2(1.f / bloomB_.w, 1.f / bloomB_.h));
    for (int i = 0; i < 2; ++i) {
        bloomC_.bind();
        bloomBlur_.set("u_dir", vec2(1.f, 0.f));
        glBindTexture(GL_TEXTURE_2D, bloomB_.colorTex);
        fullscreen();
        bloomB_.bind();
        bloomBlur_.set("u_dir", vec2(0.f, 1.f));
        glBindTexture(GL_TEXTURE_2D, bloomC_.colorTex);
        fullscreen();
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Renderer::composite(const Mission& m) {
    const Environment& e = *env_;
    const World& w = m.w;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, W_, H_);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    composite_.use();
    composite_.set("u_scene", 0);
    composite_.set("u_bloom", 1);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, scene_res_.colorTex);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, bloomB_.colorTex);
    composite_.set("u_exposure", e.exposure);
    composite_.set("u_bloomAmount", glm::mix(0.28f, 0.55f, e.nightness));
    composite_.set("u_time", w.time);
    composite_.set("u_vignette", 0.85f);
    composite_.set("u_grain", glm::mix(0.03f, 0.075f, e.nightness));
    composite_.set("u_flicker", w.flicker * 0.35f);
    composite_.set("u_static", glm::clamp(w.player.staticBurst * 2.5f, 0.f, 1.f) * (dev_.active ? 0.f : 1.f));
    composite_.set("u_saturation", glm::mix(0.95f, 0.62f, e.nightness) * (1.f - 0.2f * e.overcast));
    composite_.set("u_shadowTint", vec3(0.004f, 0.009f, 0.010f));
    composite_.set("u_texel", vec2(1.f / W_, 1.f / H_));
    composite_.set("u_aberration", 0.006f + 0.03f * w.flicker);
    fullscreen();
}

void Renderer::drawDeviceImage(const Mission& m, const DeviceLayout& d) {
    const Environment& e = *env_;
    const Player& p = m.w.player;
    float W = static_cast<float>(W_), H = static_cast<float>(H_);
    int hx = static_cast<int>(d.hole.x), hy = static_cast<int>(H - d.hole.w);
    int hw = static_cast<int>(d.hole.z - d.hole.x), hh = static_cast<int>(d.hole.w - d.hole.y);
    glViewport(hx, hy, hw, hh);
    glEnable(GL_SCISSOR_TEST);
    int sx = std::max(0, hx), sy = std::max(0, hy);
    int ex = std::min(W_, hx + hw), ey = std::min(H_, hy + hh);
    glScissor(sx, sy, std::max(0, ex - sx), std::max(0, ey - sy));
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    device_.use();
    bool night = e.nightness > 0.5f;
    int mode = 0;
    int degraded = 0;
    switch (d.item) {
        case Item::Binoculars:
            mode = night ? 1 : 0;
            degraded = m.scene().binoculars == SensorState::Degraded ? 1 : 0;
            break;
        case Item::Thermal:
            mode = 2;
            degraded = m.scene().thermal == SensorState::Degraded ? 1 : 0;
            break;
        default: mode = night ? 1 : 0; break;
    }
    device_.set("u_img", 0);
    device_.set("u_noise", 1);
    device_.set("u_mode", mode);
    device_.set("u_time", m.w.time);
    device_.set("u_exposure", e.exposure);
    device_.set("u_degraded", degraded);
    device_.set("u_aspect", static_cast<float>(hw) / std::max(1, hh));
    device_.set("u_circle", d.circle ? 1.f : 0.f);
    device_.set("u_palette", 0);
    device_.set("u_gain", 18.0f);
    float st = glm::clamp(p.staticBurst * 3.f, 0.f, 1.f);
    if (!d.hasImage) st = 1.f;
    device_.set("u_static", st);
    device_.set("u_zoom", d.zoom);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, d.hasImage ? zoom_res_.colorTex : scene_res_.colorTex);
    device_.set("u_texel", d.hasImage ? vec2(1.f / zoom_res_.w, 1.f / zoom_res_.h) : vec2(1.f / W, 1.f / H));
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, TextureLibrary::get().set(TexKind::Noise).albedo);
    fullscreen();
    glDisable(GL_SCISSOR_TEST);
    glViewport(0, 0, W_, H_);
}

void Renderer::drawDeviceFrame(const Mission& m, const DeviceLayout& d) {
    float H = static_cast<float>(H_);
    glViewport(0, 0, W_, H_);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    frame_.use();
    frame_.set("u_outer", vec4(d.outer.x, H - d.outer.w, d.outer.z, H - d.outer.y));
    frame_.set("u_hole", vec4(d.hole.x, H - d.hole.w, d.hole.z, H - d.hole.y));
    frame_.set("u_cornerOuter", d.cornerOuter);
    frame_.set("u_cornerHole", d.cornerHole);
    vec3 tone = vec3(0.17f, 0.18f, 0.15f);
    if (d.style == 1) tone = vec3(0.36f, 0.31f, 0.17f);
    if (d.style == 2) tone = vec3(0.13f, 0.14f, 0.15f);
    if (d.style == 3) tone = vec3(0.11f, 0.115f, 0.12f);
    frame_.set("u_tone", tone);
    frame_.set("u_style", d.style);
    frame_.set("u_time", m.w.time);
    frame_.set("u_blackout", d.blackout);
    frame_.set("u_alpha", 1.f);
    frame_.set("u_noise", 0);
    frame_.set("u_ledState", m.itemUsable(d.item) ? 1.f : 0.f);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, TextureLibrary::get().set(TexKind::Noise).albedo);
    fullscreen();
    glDisable(GL_BLEND);
}

// ---------------------------------------------------------------------------------------------
void Renderer::render(const Mission& m, float dt) {
    time_ += dt;
    const World& w = m.w;
    dev_ = layout(m);
    gatherLights(m);
    shadowPass(m);

    vec3 eye = w.player.eye();
    vec3 dir = m.aimDir();
    float shake = w.shake;
    if (shake > 0.001f) {
        uint32_t k = static_cast<uint32_t>(w.time * 45.f);
        dir = glm::normalize(dir + vec3(hash01(k) - 0.5f, hash01(k + 31u) - 0.5f, hash01(k + 77u) - 0.5f) * 0.03f * shake);
        eye += vec3(hash01(k + 5u) - 0.5f, hash01(k + 9u) - 0.5f, hash01(k + 13u) - 0.5f) * 0.05f * shake;
    }
    Camera base = makeCamera(eye, dir, radians(kBaseFovY), W_, H_, 0.1f);
    scene_msaa_.bind();
    drawScene(base, false, m, true);
    resolve(scene_msaa_, scene_res_);

    if (dev_.active && dev_.hasImage) {
        int zw = static_cast<int>(dev_.hole.z - dev_.hole.x);
        int zh = static_cast<int>(dev_.hole.w - dev_.hole.y);
        float scaleDown = 0.8f;
        if (ensureZoomTarget(static_cast<int>(zw * scaleDown), static_cast<int>(zh * scaleDown))) {
            zoomCam_ = makeCamera(eye, dir, dev_.fovY, zoom_msaa_.w, zoom_msaa_.h, 0.4f);
            dev_.viewProj = zoomCam_.vp;
            dev_.camPos = eye;
            zoom_msaa_.bind();
            drawScene(zoomCam_, dev_.item == Item::Thermal, m, false);
            resolve(zoom_msaa_, zoom_res_);
        } else {
            dev_.hasImage = false;
        }
    }
    bloomPass();
    composite(m);
    if (dev_.active) {
        if (dev_.style != 2) drawDeviceImage(m, dev_);
        drawDeviceFrame(m, dev_);
    }
    ui_.begin(W_, H_);
    hud_.draw(ui_, m, dev_, time_);
    ui_.flush();
}

}  // namespace aegis
