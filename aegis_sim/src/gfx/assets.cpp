#include "gfx/assets.h"

#include "core/paths.h"
#include "gfx/noise.h"
#include "gfx/textures.h"

namespace aegis {

// File names, normalised sizes and anchors.  Keep in sync with requirements.txt.
static const ModelSpec kSpecs[static_cast<int>(AssetId::Count)] = {
    /* Drone          */ {"drone_quad.glb", 1.20f, Anchor::Center, 0.50f},
    /* Rifle          */ {"rifle.glb", 1.05f, Anchor::None, 0.30f},
    /* Launcher       */ {"launcher.glb", 1.25f, Anchor::None, 0.30f},
    /* Missile        */ {"missile.glb", 1.40f, Anchor::Center, 0.85f},
    /* TreePine       */ {"tree_pine.glb", 13.0f, Anchor::GroundCenter, 0.42f},
    /* TreeBroadleaf  */ {"tree_broadleaf.glb", 10.0f, Anchor::GroundCenter, 0.42f},
    /* HouseSmall     */ {"house_small.glb", 9.0f, Anchor::GroundCenter, 0.48f},
    /* ApartmentBlock */ {"apartment_block.glb", 30.0f, Anchor::GroundCenter, 0.50f},
    /* Barn           */ {"barn.glb", 16.0f, Anchor::GroundCenter, 0.46f},
    /* UtilityPole    */ {"utility_pole.glb", 10.0f, Anchor::GroundCenter, 0.40f},
    /* Lighthouse     */ {"lighthouse.glb", 32.0f, Anchor::GroundCenter, 0.46f},
    /* RockCluster    */ {"rock_cluster.glb", 6.0f, Anchor::GroundCenter, 0.40f},
};

const ModelSpec& assetSpec(AssetId id) { return kSpecs[static_cast<int>(id)]; }

Material makeMaterial(int texKind, vec3 tint, float roughness, float metallic, float uvScale) {
    auto& lib = TextureLibrary::get();
    const TexSet& s = lib.set(static_cast<TexKind>(texKind));
    Material m;
    m.albedo = s.albedo;
    m.normal = s.normal;
    m.orm = s.orm;
    m.emissive = lib.black();
    m.baseColor = vec4(tint, 1.f);
    m.roughness = roughness;
    m.metallic = metallic;
    m.uvScale = uvScale;
    m.aoFromOrm = true;
    m.procedural = true;
    return m;
}

namespace {

using namespace build;

mat4 T(float x, float y, float z) { return glm::translate(mat4(1.f), vec3(x, y, z)); }
mat4 R(float a, vec3 axis) { return glm::rotate(mat4(1.f), a, axis); }
mat4 S(float x, float y, float z) { return glm::scale(mat4(1.f), vec3(x, y, z)); }

Material emissiveMat(vec3 color, vec3 emit) {
    auto& lib = TextureLibrary::get();
    Material m;
    m.albedo = lib.white();
    m.normal = lib.flatNormal();
    m.orm = lib.whiteLinear();
    m.emissive = lib.white();
    m.baseColor = vec4(color, 1.f);
    m.emissiveFactor = emit;
    m.roughness = 0.3f;
    m.metallic = 0.0f;
    m.aoFromOrm = false;
    return m;
}

MeshData gableRoof(float w, float d, float h, float overhang) {
    // prism along Z, ridge on top; base at y=0
    MeshData m;
    float hw = w * 0.5f + overhang, hd = d * 0.5f + overhang;
    float slant = std::sqrt(hw * hw + h * h);
    auto face = [&](vec3 a, vec3 b, vec3 c, vec3 dd, vec3 n, float lu, float lv) {
        uint32_t base = static_cast<uint32_t>(m.v.size());
        vec3 p[4] = {a, b, c, dd};
        vec2 uv[4] = {{0, 0}, {lu, 0}, {lu, lv}, {0, lv}};
        for (int k = 0; k < 4; ++k) {
            Vertex v;
            v.pos = p[k];
            v.nrm = n;
            v.uv = uv[k];
            m.v.push_back(v);
        }
        m.i.insert(m.i.end(), {base, base + 1, base + 2, base, base + 2, base + 3});
    };
    vec3 nl = glm::normalize(vec3(-h, hw, 0)), nr = glm::normalize(vec3(h, hw, 0));
    face(vec3(-hw, 0, hd), vec3(-hw, 0, -hd), vec3(0, h, -hd), vec3(0, h, hd), nl, 2 * hd, slant);
    face(vec3(hw, 0, -hd), vec3(hw, 0, hd), vec3(0, h, hd), vec3(0, h, -hd), nr, 2 * hd, slant);
    // gable ends (triangles)
    auto tri = [&](vec3 a, vec3 b, vec3 c, vec3 n) {
        uint32_t base = static_cast<uint32_t>(m.v.size());
        vec3 p[3] = {a, b, c};
        for (int k = 0; k < 3; ++k) {
            Vertex v;
            v.pos = p[k];
            v.nrm = n;
            v.uv = vec2(p[k].x, p[k].y);
            m.v.push_back(v);
        }
        m.i.insert(m.i.end(), {base, base + 1, base + 2});
    };
    tri(vec3(-hw, 0, hd), vec3(hw, 0, hd), vec3(0, h, hd), vec3(0, 0, 1));
    tri(vec3(hw, 0, -hd), vec3(-hw, 0, -hd), vec3(0, h, -hd), vec3(0, 0, -1));
    m.computeTangents();
    return m;
}

std::unique_ptr<Model> fbDrone() {
    auto m = std::make_unique<Model>();
    int carbon = m->addMaterial(makeMaterial(static_cast<int>(TexKind::PaintedMetal), vec3(1.3f, 1.4f, 1.6f), 0.45f, 0.35f, 2.0f));
    int shell = m->addMaterial(makeMaterial(static_cast<int>(TexKind::PaintedMetal), vec3(7.0f, 6.0f, 7.5f), 0.5f, 0.15f, 2.0f));
    int blade = m->addMaterial(makeMaterial(static_cast<int>(TexKind::PaintedMetal), vec3(0.5f), 0.6f, 0.0f, 4.0f));
    int lens = m->addMaterial(emissiveMat(vec3(0.02f, 0.03f, 0.05f), vec3(0.0f)));
    // body shell + battery
    m->addPart("body", box(vec3(0.20f, 0.06f, 0.30f), 4.0f), shell, T(0, 0.01f, 0), 0.55f);
    m->addPart("battery", box(vec3(0.12f, 0.05f, 0.14f), 4.0f), carbon, T(0, 0.055f, 0.02f), 0.95f);
    m->addPart("body_nose", box(vec3(0.10f, 0.05f, 0.08f), 4.0f), shell, T(0, 0.01f, -0.18f), 0.55f);
    // gimbal camera
    m->addPart("camera", sphere(0.04f, 14, 8, 4.0f), lens, T(0, -0.05f, -0.15f), 0.5f);
    // arms + motors + rotors
    const float a = 0.30f;
    const vec2 pos[4] = {{-a, -a}, {a, -a}, {-a, a}, {a, a}};
    const char* suffix[4] = {"fl", "fr", "bl", "br"};
    for (int k = 0; k < 4; ++k) {
        float ang = std::atan2(pos[k].x, pos[k].y);
        float len = std::sqrt(pos[k].x * pos[k].x + pos[k].y * pos[k].y);
        mat4 arm = T(pos[k].x * 0.5f, 0.015f, pos[k].y * 0.5f) * R(ang, vec3(0, 1, 0));
        m->addPart(std::string("arm_") + suffix[k], box(vec3(0.035f, 0.025f, len), 4.0f), carbon, arm, 0.5f);
        m->addPart(std::string("motor_") + suffix[k], cylinder(0.032f, 0.045f, 16, true, true, true, 4.0f), carbon,
                   T(pos[k].x, 0.0f, pos[k].y), 1.0f);
        MeshData rotor;
        rotor.append(box(vec3(0.34f, 0.004f, 0.026f), 6.0f));
        rotor.append(box(vec3(0.34f, 0.004f, 0.026f), 6.0f), R(kPi * 0.5f, vec3(0, 1, 0)));
        m->addPart(std::string("rotor_") + suffix[k], rotor, blade, T(pos[k].x, 0.058f, pos[k].y), 0.62f, (k == 0 || k == 3) ? 1 : -1);
        m->addPart(std::string("leg_") + suffix[k], cylinder(0.006f, 0.07f, 6, true, false, false, 4.0f), carbon,
                   T(pos[k].x * 0.55f, -0.07f, pos[k].y * 0.55f), 0.4f);
    }
    // scale to catalogue size and centre
    m->finalizeBounds();
    vec3 e = m->bmax - m->bmin;
    float s = assetSpec(AssetId::Drone).size / std::max(e.x, std::max(e.y, e.z));
    vec3 c = m->center();
    mat4 root = S(s, s, s) * T(-c.x, -c.y, -c.z);
    for (auto& p : m->parts) p.local = root * p.local;
    m->finalizeBounds();
    return m;
}

std::unique_ptr<Model> fbRifle() {
    auto m = std::make_unique<Model>();
    int steel = m->addMaterial(makeMaterial(static_cast<int>(TexKind::PaintedMetal), vec3(4.0f, 3.4f, 4.6f), 0.4f, 0.8f, 2.0f));
    int wood = m->addMaterial(makeMaterial(static_cast<int>(TexKind::Wood), vec3(0.9f), 0.6f, 0.0f, 2.0f));
    int fab = m->addMaterial(makeMaterial(static_cast<int>(TexKind::Fabric), vec3(0.8f), 0.9f, 0.0f, 4.0f));
    int glassM = m->addMaterial(emissiveMat(vec3(0.02f, 0.04f, 0.05f), vec3(0.0f)));
    // forward = -Z
    m->addPart("receiver", box(vec3(0.055f, 0.075f, 0.34f), 4.0f), steel, T(0, 0, -0.05f), 0.3f);
    m->addPart("barrel", cylinder(0.011f, 0.60f, 14, true, true, true, 4.0f), steel, T(0, 0.012f, -0.20f) * R(-kPi * 0.5f, vec3(1, 0, 0)), 0.3f);
    m->addPart("muzzle", cylinder(0.017f, 0.07f, 14, true, true, true, 4.0f), steel, T(0, 0.012f, -0.80f) * R(-kPi * 0.5f, vec3(1, 0, 0)), 0.3f);
    m->addPart("handguard", box(vec3(0.05f, 0.05f, 0.26f), 3.0f), wood, T(0, 0.0f, -0.36f), 0.3f);
    m->addPart("stock", box(vec3(0.045f, 0.10f, 0.28f), 3.0f), wood, T(0, -0.03f, 0.26f) * R(0.12f, vec3(1, 0, 0)), 0.3f);
    m->addPart("cheek", box(vec3(0.05f, 0.03f, 0.18f), 3.0f), fab, T(0, 0.06f, 0.22f), 0.3f);
    m->addPart("grip", box(vec3(0.035f, 0.11f, 0.05f), 3.0f), wood, T(0, -0.085f, 0.08f) * R(0.3f, vec3(1, 0, 0)), 0.3f);
    m->addPart("magazine", box(vec3(0.04f, 0.14f, 0.07f), 3.0f), steel, T(0, -0.10f, -0.06f) * R(-0.12f, vec3(1, 0, 0)), 0.3f);
    m->addPart("scope", cylinder(0.025f, 0.30f, 20, true, true, true, 4.0f), steel, T(0, 0.095f, -0.05f) * R(-kPi * 0.5f, vec3(1, 0, 0)), 0.3f);
    m->addPart("scope_front", cylinder(0.034f, 0.07f, 20, true, true, true, 4.0f), steel, T(0, 0.095f, -0.20f) * R(-kPi * 0.5f, vec3(1, 0, 0)), 0.3f);
    m->addPart("scope_lens", cylinder(0.030f, 0.004f, 20, true, true, true, 4.0f), glassM, T(0, 0.095f, -0.268f) * R(-kPi * 0.5f, vec3(1, 0, 0)), 0.3f);
    m->addPart("scope_mount", box(vec3(0.03f, 0.03f, 0.18f), 4.0f), steel, T(0, 0.055f, -0.05f), 0.3f);
    m->finalizeBounds();
    vec3 e = m->bmax - m->bmin;
    float s = assetSpec(AssetId::Rifle).size / std::max(e.x, std::max(e.y, e.z));
    mat4 root = S(s, s, s);
    for (auto& p : m->parts) p.local = root * p.local;
    m->finalizeBounds();
    return m;
}

std::unique_ptr<Model> fbLauncher() {
    auto m = std::make_unique<Model>();
    int olive = m->addMaterial(makeMaterial(static_cast<int>(TexKind::PaintedMetal), vec3(3.6f), 0.55f, 0.1f, 1.5f));
    int dark = m->addMaterial(makeMaterial(static_cast<int>(TexKind::PaintedMetal), vec3(1.6f), 0.45f, 0.5f, 2.0f));
    int pad = m->addMaterial(makeMaterial(static_cast<int>(TexKind::Fabric), vec3(0.7f), 0.95f, 0.0f, 4.0f));
    int glassM = m->addMaterial(emissiveMat(vec3(0.02f, 0.03f, 0.06f), vec3(0.0f)));
    auto along = [](float z, float y = 0.f) { return T(0, y, z) * R(-kPi * 0.5f, vec3(1, 0, 0)); };
    m->addPart("tube", cylinder(0.052f, 1.05f, 24, true, false, false, 1.5f), olive, along(-0.55f) * T(0, 0, 0), 0.3f);
    m->addPart("tube_front", cylinder(0.068f, 0.12f, 24, true, true, true, 1.5f), dark, along(-0.65f), 0.3f);
    m->addPart("tube_rear", cylinder(0.075f, 0.14f, 24, true, true, true, 1.5f), dark, along(0.43f), 0.3f);
    m->addPart("seeker", box(vec3(0.10f, 0.09f, 0.22f), 3.0f), dark, T(0.0f, 0.12f, -0.30f), 0.4f);
    m->addPart("seeker_lens", cylinder(0.035f, 0.01f, 18, true, true, true, 3.0f), glassM, T(0, 0.12f, -0.415f) * R(-kPi * 0.5f, vec3(1, 0, 0)), 0.3f);
    m->addPart("grip", box(vec3(0.045f, 0.14f, 0.06f), 3.0f), dark, T(0, -0.10f, -0.05f) * R(0.25f, vec3(1, 0, 0)), 0.3f);
    m->addPart("grip_front", box(vec3(0.04f, 0.12f, 0.05f), 3.0f), dark, T(0, -0.09f, -0.45f), 0.3f);
    m->addPart("shoulder_pad", box(vec3(0.10f, 0.12f, 0.06f), 3.0f), pad, T(0, -0.02f, 0.30f), 0.3f);
    m->addPart("band", cylinder(0.056f, 0.04f, 24, true, true, true, 1.5f), dark, along(-0.10f), 0.3f);
    m->finalizeBounds();
    vec3 e = m->bmax - m->bmin;
    float s = assetSpec(AssetId::Launcher).size / std::max(e.x, std::max(e.y, e.z));
    mat4 root = S(s, s, s);
    for (auto& p : m->parts) p.local = root * p.local;
    m->finalizeBounds();
    return m;
}

std::unique_ptr<Model> fbMissile() {
    auto m = std::make_unique<Model>();
    int body = m->addMaterial(makeMaterial(static_cast<int>(TexKind::PaintedMetal), vec3(6.0f, 6.0f, 5.7f), 0.4f, 0.2f, 2.0f));
    int nose = m->addMaterial(makeMaterial(static_cast<int>(TexKind::PaintedMetal), vec3(1.6f), 0.35f, 0.4f, 2.0f));
    auto along = [](float z) { return T(0, 0, z) * R(-kPi * 0.5f, vec3(1, 0, 0)); };
    m->addPart("body", cylinder(0.035f, 0.9f, 20, true, false, false, 2.0f), body, along(-0.15f), 0.85f);
    m->addPart("nose", cone(0.035f, 0.18f, 20, 2.0f), nose, T(0, 0, -0.9f) * R(-kPi * 0.5f, vec3(1, 0, 0)) * T(0, 0.0f, 0) * R(0, vec3(1, 0, 0)), 0.85f);
    for (int k = 0; k < 4; ++k) {
        float a = k * kPi * 0.5f + kPi * 0.25f;
        m->addPart("fin", box(vec3(0.004f, 0.10f, 0.12f), 3.0f), nose, R(a, vec3(0, 0, 1)) * T(0, 0.06f, 0.55f), 0.85f);
    }
    m->addPart("nozzle", cylinder(0.026f, 0.05f, 16, true, true, true, 2.0f), nose, along(0.78f), 1.0f);
    m->finalizeBounds();
    vec3 e = m->bmax - m->bmin;
    float s = assetSpec(AssetId::Missile).size / std::max(e.x, std::max(e.y, e.z));
    vec3 c = m->center();
    mat4 root = S(s, s, s) * T(-c.x, -c.y, -c.z);
    for (auto& p : m->parts) p.local = root * p.local;
    m->finalizeBounds();
    return m;
}

std::unique_ptr<Model> fbTreePine() {
    auto m = std::make_unique<Model>();
    int bark = m->addMaterial(makeMaterial(static_cast<int>(TexKind::Wood), vec3(0.5f, 0.38f, 0.3f), 0.9f, 0.0f, 0.5f));
    Material fo = makeMaterial(static_cast<int>(TexKind::Grass), vec3(0.28f, 0.46f, 0.30f), 0.9f, 0.0f, 0.35f);
    int foliage = m->addMaterial(fo);
    m->addPart("trunk", cylinderTapered(0.28f, 0.10f, 12.5f, 10, true, 0.5f), bark, mat4(1.f), 0.38f);
    MeshData cones;
    float y = 2.2f;
    for (int k = 0; k < 6; ++k) {
        float t = static_cast<float>(k) / 5.0f;
        float r = 3.0f * (1.0f - t * 0.82f), h = 3.8f - t * 1.0f;
        MeshData c = cone(r, h, 12, 0.35f);
        cones.append(c, T(0, y, 0) * R(0.0f, vec3(0, 1, 0)));
        y += h * 0.62f;
    }
    m->addPart("foliage", cones, foliage, mat4(1.f), 0.42f);
    m->finalizeBounds();
    return m;
}

std::unique_ptr<Model> fbTreeBroadleaf() {
    auto m = std::make_unique<Model>();
    int bark = m->addMaterial(makeMaterial(static_cast<int>(TexKind::Wood), vec3(0.45f, 0.36f, 0.3f), 0.9f, 0.0f, 0.5f));
    int foliage = m->addMaterial(makeMaterial(static_cast<int>(TexKind::Grass), vec3(0.36f, 0.52f, 0.28f), 0.9f, 0.0f, 0.3f));
    m->addPart("trunk", cylinderTapered(0.35f, 0.18f, 4.5f, 10, true, 0.5f), bark, mat4(1.f), 0.38f);
    MeshData crown;
    const vec4 blobs[] = {{0, 6.8f, 0, 3.2f}, {-2.2f, 6.0f, 0.8f, 2.4f}, {2.1f, 6.2f, -0.7f, 2.5f}, {0.3f, 8.2f, 0.6f, 2.2f}, {0.6f, 5.6f, -2.0f, 2.1f}, {-0.6f, 5.8f, 2.1f, 2.1f}};
    int k = 0;
    for (const vec4& b : blobs) {
        MeshData s = sphere(b.w, 14, 9, 0.3f);
        for (auto& v : s.v) {
            float n = noise::fbm2(v.pos.x * 0.9f + b.x * 3.1f, v.pos.y * 0.9f + b.z * 1.7f + k, 3, 2.0f, 0.5f, 7);
            v.pos *= 0.82f + 0.4f * n;
        }
        s.computeSmoothNormals();
        s.computeTangents();
        crown.append(s, T(b.x, b.y, b.z));
        ++k;
    }
    m->addPart("foliage", crown, foliage, mat4(1.f), 0.42f);
    m->finalizeBounds();
    return m;
}

std::unique_ptr<Model> fbHouse() {
    auto m = std::make_unique<Model>();
    int wall = m->addMaterial(makeMaterial(static_cast<int>(TexKind::Concrete), vec3(1.5f, 1.4f, 1.15f), 0.85f, 0.0f, 0.4f));
    int roof = m->addMaterial(makeMaterial(static_cast<int>(TexKind::PaintedMetal), vec3(6.5f, 2.0f, 2.4f), 0.6f, 0.1f, 0.5f));
    int door = m->addMaterial(makeMaterial(static_cast<int>(TexKind::Wood), vec3(0.6f), 0.7f, 0.0f, 1.0f));
    int glassOn = m->addMaterial(emissiveMat(vec3(0.05f, 0.06f, 0.08f), vec3(1.2f, 0.85f, 0.45f)));
    int glassOff = m->addMaterial(emissiveMat(vec3(0.03f, 0.04f, 0.06f), vec3(0.0f)));
    m->addPart("walls", box(vec3(8.0f, 3.2f, 6.0f), 0.4f), wall, T(0, 1.6f, 0), 0.5f);
    m->addPart("roof", gableRoof(8.0f, 6.0f, 2.3f, 0.35f), roof, T(0, 3.2f, 0), 0.5f);
    m->addPart("chimney", box(vec3(0.6f, 2.0f, 0.6f), 0.5f), wall, T(2.2f, 4.2f, -1.0f), 0.7f);
    m->addPart("door", box(vec3(1.0f, 2.1f, 0.12f), 1.0f), door, T(0, 1.05f, 3.02f), 0.45f);
    MeshData on, off;
    const float xs[3] = {-2.7f, 2.7f, 0.0f};
    for (int k = 0; k < 2; ++k) {
        (k == 0 ? on : off).append(box(vec3(1.1f, 1.0f, 0.1f), 1.0f), T(xs[k], 1.7f, 3.03f));
        off.append(box(vec3(1.1f, 1.0f, 0.1f), 1.0f), T(xs[k], 1.7f, -3.03f));
    }
    off.append(box(vec3(0.1f, 1.0f, 1.1f), 1.0f), T(4.03f, 1.7f, 0));
    on.append(box(vec3(0.1f, 1.0f, 1.1f), 1.0f), T(-4.03f, 1.7f, 0));
    m->addPart("window_lit", on, glassOn, mat4(1.f), 0.8f);
    m->addPart("window_dark", off, glassOff, mat4(1.f), 0.4f);
    m->finalizeBounds();
    return m;
}

std::unique_ptr<Model> fbApartment() {
    auto m = std::make_unique<Model>();
    int wall = m->addMaterial(makeMaterial(static_cast<int>(TexKind::Concrete), vec3(1.25f, 1.2f, 1.1f), 0.88f, 0.0f, 0.25f));
    int trim = m->addMaterial(makeMaterial(static_cast<int>(TexKind::Concrete), vec3(0.7f), 0.9f, 0.0f, 0.25f));
    int glassOn = m->addMaterial(emissiveMat(vec3(0.05f, 0.06f, 0.08f), vec3(1.1f, 0.8f, 0.42f)));
    int glassOff = m->addMaterial(emissiveMat(vec3(0.03f, 0.04f, 0.06f), vec3(0.0f)));
    const float W = 18.0f, D = 12.0f, H = 30.0f;
    m->addPart("walls", box(vec3(W, H, D), 0.25f), wall, T(0, H * 0.5f, 0), 0.5f);
    m->addPart("roof_lip", box(vec3(W + 0.6f, 0.8f, D + 0.6f), 0.25f), trim, T(0, H + 0.4f, 0), 0.5f);
    m->addPart("roof_box", box(vec3(4.0f, 2.2f, 3.0f), 0.25f), trim, T(3.0f, H + 1.9f, 1.0f), 0.6f);
    MeshData on, off;
    const int cols = 6, rows = 9;
    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            float x = -W * 0.5f + (c + 0.5f) * (W / cols);
            float y = 3.0f + r * 3.0f;
            for (int side = 0; side < 2; ++side) {
                float z = side == 0 ? D * 0.5f + 0.03f : -D * 0.5f - 0.03f;
                bool lit = noise::hash2(c + side * 7, r, 1234) > 0.6f;
                (lit ? on : off).append(box(vec3(1.5f, 1.5f, 0.08f), 1.0f), T(x, y, z));
            }
        }
    }
    for (int r = 0; r < rows; ++r)
        for (int c = 0; c < 4; ++c) {
            float z = -D * 0.5f + (c + 0.5f) * (D / 4);
            for (int side = 0; side < 2; ++side) {
                float x = side == 0 ? W * 0.5f + 0.03f : -W * 0.5f - 0.03f;
                bool lit = noise::hash2(c + side * 5, r + 40, 99) > 0.62f;
                (lit ? on : off).append(box(vec3(0.08f, 1.5f, 1.5f), 1.0f), T(x, 3.0f + r * 3.0f, z));
            }
        }
    m->addPart("window_lit", on, glassOn, mat4(1.f), 0.8f);
    m->addPart("window_dark", off, glassOff, mat4(1.f), 0.4f);
    m->finalizeBounds();
    return m;
}

std::unique_ptr<Model> fbBarn() {
    auto m = std::make_unique<Model>();
    int wood = m->addMaterial(makeMaterial(static_cast<int>(TexKind::Wood), vec3(1.2f, 0.55f, 0.42f), 0.8f, 0.0f, 0.3f));
    int roof = m->addMaterial(makeMaterial(static_cast<int>(TexKind::PaintedMetal), vec3(6.0f, 5.0f, 6.5f), 0.5f, 0.6f, 0.4f));
    int trim = m->addMaterial(makeMaterial(static_cast<int>(TexKind::Wood), vec3(1.4f, 1.35f, 1.3f), 0.8f, 0.0f, 0.5f));
    m->addPart("walls", box(vec3(10.0f, 5.5f, 16.0f), 0.3f), wood, T(0, 2.75f, 0), 0.46f);
    m->addPart("roof", gableRoof(10.0f, 16.0f, 4.2f, 0.5f), roof, T(0, 5.5f, 0), 0.5f);
    m->addPart("door_trim", box(vec3(4.2f, 4.2f, 0.15f), 0.5f), trim, T(0, 2.1f, 8.05f), 0.46f);
    m->addPart("door_cross", box(vec3(0.25f, 5.2f, 0.18f), 0.5f), trim, T(0, 2.1f, 8.1f) * R(0.7f, vec3(0, 0, 1)), 0.46f);
    m->finalizeBounds();
    return m;
}

std::unique_ptr<Model> fbPole() {
    auto m = std::make_unique<Model>();
    int wood = m->addMaterial(makeMaterial(static_cast<int>(TexKind::Wood), vec3(0.55f, 0.5f, 0.45f), 0.85f, 0.0f, 0.4f));
    int metal = m->addMaterial(makeMaterial(static_cast<int>(TexKind::PaintedMetal), vec3(5.0f), 0.4f, 0.6f, 2.0f));
    m->addPart("pole", cylinderTapered(0.17f, 0.11f, 10.0f, 10, true, 0.5f), wood, mat4(1.f), 0.40f);
    m->addPart("crossarm", box(vec3(2.4f, 0.14f, 0.14f), 0.5f), wood, T(0, 9.3f, 0), 0.40f);
    m->addPart("crossarm_low", box(vec3(1.8f, 0.12f, 0.12f), 0.5f), wood, T(0, 8.4f, 0), 0.40f);
    MeshData ins;
    for (int k = 0; k < 3; ++k) ins.append(cylinder(0.04f, 0.18f, 8, true, true, true, 3.0f), T(-1.0f + k * 1.0f, 9.37f, 0));
    m->addPart("insulators", ins, metal, mat4(1.f), 0.4f);
    m->addPart("transformer", cylinder(0.28f, 0.7f, 14, true, true, true, 2.0f), metal, T(0.35f, 7.3f, 0.0f), 0.55f);
    m->finalizeBounds();
    return m;
}

std::unique_ptr<Model> fbLighthouse() {
    auto m = std::make_unique<Model>();
    int white = m->addMaterial(makeMaterial(static_cast<int>(TexKind::Concrete), vec3(1.9f, 1.9f, 1.85f), 0.8f, 0.0f, 0.3f));
    int red = m->addMaterial(makeMaterial(static_cast<int>(TexKind::Concrete), vec3(1.5f, 0.35f, 0.28f), 0.8f, 0.0f, 0.3f));
    int metal = m->addMaterial(makeMaterial(static_cast<int>(TexKind::PaintedMetal), vec3(4.0f), 0.45f, 0.6f, 1.0f));
    int lamp = m->addMaterial(emissiveMat(vec3(0.5f, 0.45f, 0.3f), vec3(6.0f, 5.0f, 3.2f)));
    float y = 0.0f;
    for (int k = 0; k < 6; ++k) {
        float r0 = 4.0f - k * 0.45f, r1 = 4.0f - (k + 1) * 0.45f;
        m->addPart("band", cylinderTapered(r0, r1, 4.2f, 24, true, 0.3f), (k % 2 == 0) ? white : red, T(0, y, 0), 0.46f);
        y += 4.2f;
    }
    m->addPart("gallery", cylinder(2.3f, 0.5f, 24, true, true, true, 0.5f), metal, T(0, y, 0), 0.4f);
    m->addPart("lamp_room", cylinder(1.3f, 2.2f, 20, true, false, false, 0.5f), lamp, T(0, y + 0.5f, 0), 1.0f);
    m->addPart("cap", cone(1.8f, 1.6f, 20, 0.5f), red, T(0, y + 2.7f, 0), 0.42f);
    m->finalizeBounds();
    return m;
}

std::unique_ptr<Model> fbRocks() {
    auto m = std::make_unique<Model>();
    int rock = m->addMaterial(makeMaterial(static_cast<int>(TexKind::Rock), vec3(1.0f), 0.9f, 0.0f, 0.3f));
    MeshData all;
    const vec4 rocks[] = {{0, 0.6f, 0, 1.6f}, {1.9f, 0.4f, 0.8f, 1.0f}, {-1.6f, 0.4f, 1.1f, 1.1f}, {0.6f, 0.3f, -1.7f, 0.9f}, {-0.9f, 0.3f, -1.3f, 0.7f}};
    int k = 0;
    for (const vec4& r : rocks) {
        MeshData s = sphere(r.w, 16, 10, 0.3f);
        for (auto& v : s.v) {
            float n = noise::fbm2(v.pos.x * 1.3f + k * 5.1f, v.pos.z * 1.3f + v.pos.y * 0.9f, 4, 2.0f, 0.55f, 31);
            v.pos *= 0.72f + 0.55f * n;
            v.pos.y *= 0.75f;
        }
        s.computeSmoothNormals();
        s.computeTangents();
        all.append(s, T(r.x, r.y, r.z));
        ++k;
    }
    m->addPart("rocks", all, rock, mat4(1.f), 0.38f);
    m->finalizeBounds();
    return m;
}

}  // namespace

std::unique_ptr<Model> buildFallbackModel(AssetId id) {
    std::unique_ptr<Model> m;
    switch (id) {
        case AssetId::Drone: m = fbDrone(); break;
        case AssetId::Rifle: m = fbRifle(); break;
        case AssetId::Launcher: m = fbLauncher(); break;
        case AssetId::Missile: m = fbMissile(); break;
        case AssetId::TreePine: m = fbTreePine(); break;
        case AssetId::TreeBroadleaf: m = fbTreeBroadleaf(); break;
        case AssetId::HouseSmall: m = fbHouse(); break;
        case AssetId::ApartmentBlock: m = fbApartment(); break;
        case AssetId::Barn: m = fbBarn(); break;
        case AssetId::UtilityPole: m = fbPole(); break;
        case AssetId::Lighthouse: m = fbLighthouse(); break;
        case AssetId::RockCluster: m = fbRocks(); break;
        default: break;
    }
    if (m) {
        m->name = std::string(assetSpec(id).file) + " (built-in)";
        m->fromFile = false;
    }
    return m;
}

void AssetCatalog::load(const std::string& resourceDir) {
    for (int i = 0; i < static_cast<int>(AssetId::Count); ++i) {
        AssetId id = static_cast<AssetId>(i);
        const ModelSpec& spec = assetSpec(id);
        std::unique_ptr<Model> m = loadGLB(resourceDir + "/" + spec.file, spec);
        if (!m) {
            if (fileExists(resourceDir + "/" + spec.file)) LOG_WARN("%s is invalid; using built-in model", spec.file);
            else LOG_INFO("%s not found; using built-in model", spec.file);
            m = buildFallbackModel(id);
        }
        models_[i] = std::move(m);
    }
}

}  // namespace aegis
