#include "gfx/cab.h"

#include "gfx/assets.h"
#include "gfx/textures.h"

namespace aegis {

namespace {
mat4 T(float x, float y, float z) { return glm::translate(mat4(1.0f), vec3(x, y, z)); }
mat4 R(float a, vec3 ax) { return glm::rotate(mat4(1.0f), a, ax); }
}  // namespace

std::unique_ptr<Model> buildCab() {
    auto m = std::make_unique<Model>();
    m->name = "cab";
    const int K_CONC = static_cast<int>(TexKind::Concrete);
    const int K_METAL = static_cast<int>(TexKind::PaintedMetal);
    const int K_WOOD = static_cast<int>(TexKind::Wood);
    const int K_FAB = static_cast<int>(TexKind::Fabric);

    int paint = m->addMaterial(makeMaterial(K_METAL, vec3(7.5f, 7.5f, 6.8f), 0.55f, 0.1f, 1.2f));      // olive drab
    int paintDark = m->addMaterial(makeMaterial(K_METAL, vec3(3.0f, 3.1f, 2.9f), 0.5f, 0.2f, 1.5f));
    int steel = m->addMaterial(makeMaterial(K_METAL, vec3(8.5f, 7.0f, 11.0f), 0.45f, 0.5f, 1.5f));
    int conc = m->addMaterial(makeMaterial(K_CONC, vec3(1.1f, 1.08f, 1.0f), 0.9f, 0.0f, 0.7f));
    int wood = m->addMaterial(makeMaterial(K_WOOD, vec3(0.85f, 0.75f, 0.6f), 0.65f, 0.0f, 0.9f));
    int woodDark = m->addMaterial(makeMaterial(K_WOOD, vec3(0.45f, 0.35f, 0.28f), 0.7f, 0.0f, 0.9f));
    int fabric = m->addMaterial(makeMaterial(K_FAB, vec3(0.5f, 0.55f, 0.45f), 0.95f, 0.0f, 2.0f));
    auto emissiveMat = [&](vec3 base, vec3 emis) {
        Material mt = makeMaterial(K_METAL, base, 0.35f, 0.1f, 4.0f);
        mt.emissiveFactor = emis;
        mt.emissive = TextureLibrary::get().white();
        return m->addMaterial(mt);
    };
    int screenMat = emissiveMat(vec3(0.02f, 0.03f, 0.03f), vec3(0.08f, 0.55f, 0.3f));
    int screenMat2 = emissiveMat(vec3(0.02f, 0.03f, 0.03f), vec3(0.05f, 0.25f, 0.5f));
    int bulbMat = emissiveMat(vec3(0.9f, 0.85f, 0.7f), vec3(6.0f, 4.6f, 2.8f));
    int alarmMat = emissiveMat(vec3(0.4f, 0.02f, 0.02f), vec3(6.0f, 0.1f, 0.05f));
    int ledMat = emissiveMat(vec3(0.02f, 0.1f, 0.02f), vec3(0.2f, 2.5f, 0.3f));

    using namespace build;
    const float H = 1.7f;                 // half size of the cab
    const float fy = kCabFloorY;
    const float sill = fy + 0.95f;
    const float head = fy + 2.5f;
    const float roofY = fy + 2.9f;

    // floor + catwalk -------------------------------------------------------------------------
    m->addPart("floor", box(vec3(2 * H + 0.2f, 0.16f, 2 * H + 0.2f), 0.9f), wood, T(0, fy - 0.08f, 0), 0.30f);
    m->addPart("catwalk", box(vec3(2 * H + 2.0f, 0.10f, 2 * H + 2.0f), 0.32f), steel, T(0, fy - 0.2f, 0), 0.30f);
    // floorboards details: strips
    for (int i = -3; i <= 3; ++i)
        m->addPart("plank", box(vec3(0.02f, 0.01f, 2 * H), 2.0f), woodDark, T(i * 0.46f, fy + 0.005f, 0), 0.3f);

    // lower walls (waist high) ------------------------------------------------------------------
    float wallT = 0.14f;
    float wallH = sill - fy;
    m->addPart("wall_n", box(vec3(2 * H, wallH, wallT), 0.48f), paint, T(0, fy + wallH * 0.5f, -H), 0.32f);
    m->addPart("wall_s", box(vec3(2 * H, wallH, wallT), 0.48f), paint, T(0, fy + wallH * 0.5f, H), 0.32f);
    m->addPart("wall_e", box(vec3(wallT, wallH, 2 * H), 0.48f), paint, T(H, fy + wallH * 0.5f, 0), 0.32f);
    m->addPart("wall_w", box(vec3(wallT, wallH, 2 * H), 0.48f), paint, T(-H, fy + wallH * 0.5f, 0), 0.32f);
    // window sill caps (wood) with a slight overhang
    m->addPart("sill_n", box(vec3(2 * H + 0.1f, 0.05f, 0.30f), 1.5f), wood, T(0, sill + 0.025f, -H), 0.3f);
    m->addPart("sill_s", box(vec3(2 * H + 0.1f, 0.05f, 0.30f), 1.5f), wood, T(0, sill + 0.025f, H), 0.3f);
    m->addPart("sill_e", box(vec3(0.30f, 0.05f, 2 * H + 0.1f), 1.5f), wood, T(H, sill + 0.025f, 0), 0.3f);
    m->addPart("sill_w", box(vec3(0.30f, 0.05f, 2 * H + 0.1f), 1.5f), wood, T(-H, sill + 0.025f, 0), 0.3f);
    // wall panel ribs
    for (int k = -2; k <= 2; ++k) {
        float o = k * 0.62f;
        m->addPart("rib", box(vec3(0.05f, wallH, 0.04f), 0.80f), paintDark, T(o, fy + wallH * 0.5f, -H + 0.09f), 0.3f);
        m->addPart("rib", box(vec3(0.05f, wallH, 0.04f), 0.80f), paintDark, T(o, fy + wallH * 0.5f, H - 0.09f), 0.3f);
        m->addPart("rib", box(vec3(0.04f, wallH, 0.05f), 0.80f), paintDark, T(-H + 0.09f, fy + wallH * 0.5f, o), 0.3f);
        m->addPart("rib", box(vec3(0.04f, wallH, 0.05f), 0.80f), paintDark, T(H - 0.09f, fy + wallH * 0.5f, o), 0.3f);
    }

    // posts, headers, roof ------------------------------------------------------------------------
    for (int sx = -1; sx <= 1; sx += 2)
        for (int sz = -1; sz <= 1; sz += 2)
            m->addPart("post", box(vec3(0.16f, head - fy + 0.4f, 0.16f), 0.60f), paintDark, T(sx * H, fy + (head - fy + 0.4f) * 0.5f, sz * H), 0.30f);
    float hh = roofY - head;
    m->addPart("head_n", box(vec3(2 * H + 0.5f, hh, 0.2f), 0.48f), paintDark, T(0, head + hh * 0.5f, -H), 0.3f);
    m->addPart("head_s", box(vec3(2 * H + 0.5f, hh, 0.2f), 0.48f), paintDark, T(0, head + hh * 0.5f, H), 0.3f);
    m->addPart("head_e", box(vec3(0.2f, hh, 2 * H + 0.5f), 0.48f), paintDark, T(H, head + hh * 0.5f, 0), 0.3f);
    m->addPart("head_w", box(vec3(0.2f, hh, 2 * H + 0.5f), 0.48f), paintDark, T(-H, head + hh * 0.5f, 0), 0.3f);
    m->addPart("ceiling", box(vec3(2 * H + 0.8f, 0.16f, 2 * H + 0.8f), 0.8f), conc, T(0, roofY + 0.08f, 0), 0.3f);
    // ceiling beams
    for (int i = -1; i <= 1; ++i)
        m->addPart("beam", box(vec3(0.12f, 0.12f, 2 * H), 1.5f), woodDark, T(i * 1.0f, roofY - 0.06f, 0), 0.3f);
    // fascia + antenna on the roof (visible when looking up)
    m->addPart("antenna", cylinder(0.02f, 1.6f, 8, true, true, true, 0.80f), steel, T(1.0f, roofY + 0.16f, 1.0f), 0.3f);

    // lamp -----------------------------------------------------------------------------------------
    vec3 lp = cabLampPos();
    m->addPart("lamp_cord", cylinder(0.008f, 0.45f, 6, true, false, false, 0.80f), paintDark, T(lp.x, roofY - 0.4f, lp.z), 0.3f);
    m->addPart("lamp_shade", cylinderTapered(0.06f, 0.22f, 0.16f, 20, true, 0.80f), paintDark, T(lp.x, lp.y - 0.05f, lp.z), 0.55f);
    m->addPart("lamp_bulb", sphere(0.045f, 12, 8, 2.0f), bulbMat, T(lp.x, lp.y - 0.08f, lp.z), 0.9f);

    // desk with equipment (south side, +Z) -------------------------------------------------------------
    float dz = H - 0.42f;
    m->addPart("desk_top", box(vec3(1.7f, 0.05f, 0.7f), 1.2f), wood, T(0.0f, fy + 0.78f, dz), 0.3f);
    m->addPart("desk_front", box(vec3(1.7f, 0.7f, 0.04f), 1.2f), woodDark, T(0.0f, fy + 0.4f, dz - 0.33f), 0.3f);
    for (int sx = -1; sx <= 1; sx += 2) m->addPart("desk_leg", box(vec3(0.06f, 0.75f, 0.6f), 1.5f), woodDark, T(sx * 0.8f, fy + 0.375f, dz), 0.3f);
    // monitors
    m->addPart("monitor_body_l", box(vec3(0.46f, 0.34f, 0.07f), 0.80f), paintDark, T(-0.5f, fy + 1.04f, dz + 0.1f) * R(0.25f, vec3(0, 1, 0)), 0.55f);
    m->addPart("monitor_screen_l", box(vec3(0.40f, 0.28f, 0.01f), 2.0f), screenMat, T(-0.5f, fy + 1.04f, dz + 0.1f - 0.035f) * R(0.25f, vec3(0, 1, 0)) * T(0, 0, -0.0f), 0.8f);
    m->addPart("monitor_body_r", box(vec3(0.36f, 0.28f, 0.07f), 0.80f), paintDark, T(0.45f, fy + 1.01f, dz + 0.12f) * R(-0.3f, vec3(0, 1, 0)), 0.55f);
    m->addPart("monitor_screen_r", box(vec3(0.31f, 0.22f, 0.01f), 2.0f), screenMat2, T(0.45f, fy + 1.01f, dz + 0.12f - 0.035f) * R(-0.3f, vec3(0, 1, 0)), 0.8f);
    m->addPart("monitor_stand", box(vec3(0.05f, 0.14f, 0.05f), 0.80f), paintDark, T(-0.5f, fy + 0.88f, dz + 0.12f), 0.4f);
    m->addPart("monitor_stand", box(vec3(0.05f, 0.12f, 0.05f), 0.80f), paintDark, T(0.45f, fy + 0.87f, dz + 0.13f), 0.4f);
    // radio set
    m->addPart("radio", box(vec3(0.30f, 0.16f, 0.2f), 0.80f), paint, T(0.05f, fy + 0.88f, dz + 0.15f), 0.5f);
    m->addPart("radio_dial", cylinder(0.025f, 0.02f, 12, true, true, true, 0.80f), steel, T(-0.03f, fy + 0.96f, dz + 0.05f), 0.4f);
    m->addPart("radio_dial", cylinder(0.025f, 0.02f, 12, true, true, true, 0.80f), steel, T(0.13f, fy + 0.96f, dz + 0.05f), 0.4f);
    m->addPart("radio_led", sphere(0.008f, 6, 4, 2.0f), ledMat, T(0.2f, fy + 0.93f, dz + 0.049f), 0.7f);
    m->addPart("radio_ant", cylinder(0.005f, 0.5f, 6, true, false, false, 0.80f), steel, T(0.18f, fy + 0.96f, dz + 0.22f), 0.3f);
    // alarm beacon
    m->addPart("alarm_base", cylinder(0.05f, 0.03f, 14, true, true, true, 0.80f), paintDark, T(0.75f, fy + 0.805f, dz + 0.2f), 0.4f);
    m->addPart("alarm_lamp", sphere(0.045f, 12, 8, 2.0f), alarmMat, T(0.75f, fy + 0.86f, dz + 0.2f), 0.5f);
    // coffee mug, papers, clipboard
    m->addPart("mug", cylinder(0.04f, 0.09f, 14, true, true, true, 0.80f), steel, T(-0.82f, fy + 0.805f, dz - 0.1f), 0.7f);
    m->addPart("paper", box(vec3(0.21f, 0.004f, 0.29f), 3.0f), conc, T(0.38f, fy + 0.806f, dz - 0.15f) * R(0.3f, vec3(0, 1, 0)), 0.3f);
    m->addPart("paper", box(vec3(0.21f, 0.004f, 0.29f), 3.0f), conc, T(0.5f, fy + 0.81f, dz - 0.12f) * R(-0.15f, vec3(0, 1, 0)), 0.3f);
    // chair behind the player (fabric)
    m->addPart("chair_seat", box(vec3(0.5f, 0.07f, 0.5f), 2.0f), fabric, T(0.0f, fy + 0.5f, dz - 0.9f), 0.35f);
    m->addPart("chair_back", box(vec3(0.5f, 0.55f, 0.06f), 2.0f), fabric, T(0.0f, fy + 0.8f, dz - 1.15f), 0.35f);
    m->addPart("chair_post", cylinder(0.025f, 0.45f, 8, true, false, false, 0.80f), steel, T(0.0f, fy + 0.04f, dz - 0.9f), 0.3f);
    // equipment locker (west)
    m->addPart("locker", box(vec3(0.5f, 1.8f, 0.45f), 0.48f), paint, T(-H + 0.3f, fy + 0.9f, -0.6f), 0.35f);
    m->addPart("locker_vent", box(vec3(0.36f, 0.02f, 0.01f), 1.60f), paintDark, T(-H + 0.3f, fy + 1.55f, -0.6f + 0.226f), 0.35f);
    m->addPart("locker_vent", box(vec3(0.36f, 0.02f, 0.01f), 1.60f), paintDark, T(-H + 0.3f, fy + 1.6f, -0.6f + 0.226f), 0.35f);

    // railing around the catwalk -----------------------------------------------------------------
    float rr = H + 0.95f;
    float rh = 1.05f;
    for (int i = -4; i <= 4; ++i) {
        float o = i * rr / 4.0f;
        m->addPart("rail_post", box(vec3(0.05f, rh, 0.05f), 0.80f), steel, T(o, fy - 0.15f + rh * 0.5f, -rr), 0.3f);
        m->addPart("rail_post", box(vec3(0.05f, rh, 0.05f), 0.80f), steel, T(o, fy - 0.15f + rh * 0.5f, rr), 0.3f);
        if (std::abs(i) < 4) {
            m->addPart("rail_post", box(vec3(0.05f, rh, 0.05f), 0.80f), steel, T(-rr, fy - 0.15f + rh * 0.5f, o), 0.3f);
            m->addPart("rail_post", box(vec3(0.05f, rh, 0.05f), 0.80f), steel, T(rr, fy - 0.15f + rh * 0.5f, o), 0.3f);
        }
    }
    for (float yy : {0.55f, 1.05f}) {
        float y = fy - 0.15f + yy;
        m->addPart("rail", box(vec3(2 * rr, 0.04f, 0.04f), 0.80f), steel, T(0, y, -rr), 0.3f);
        m->addPart("rail", box(vec3(2 * rr, 0.04f, 0.04f), 0.80f), steel, T(0, y, rr), 0.3f);
        m->addPart("rail", box(vec3(0.04f, 0.04f, 2 * rr), 0.80f), steel, T(-rr, y, 0), 0.3f);
        m->addPart("rail", box(vec3(0.04f, 0.04f, 2 * rr), 0.80f), steel, T(rr, y, 0), 0.3f);
    }

    // tower legs and cross bracing ---------------------------------------------------------------------
    float base = 3.2f;   // legs flare out toward the ground
    for (int sx = -1; sx <= 1; sx += 2)
        for (int sz = -1; sz <= 1; sz += 2) {
            vec3 top(sx * (H + 0.6f), fy - 0.25f, sz * (H + 0.6f));
            vec3 bot(sx * base, 0.0f, sz * base);
            vec3 d = top - bot;
            float len = glm::length(d);
            vec3 dir = d / len;
            vec3 axis = glm::cross(vec3(0, 1, 0), dir);
            float ang = std::acos(glm::clamp(dir.y, -1.f, 1.f));
            mat4 mat = T(bot.x, bot.y, bot.z) * (glm::length(axis) > 1e-4f ? glm::rotate(mat4(1.0f), ang, glm::normalize(axis)) : mat4(1.0f));
            m->addPart("leg", box(vec3(0.16f, len, 0.16f), 0.60f), steel, mat * T(0, len * 0.5f, 0), 0.3f);
            // foot plate
            m->addPart("foot", box(vec3(0.6f, 0.2f, 0.6f), 1.2f), conc, T(bot.x, 0.1f, bot.z), 0.3f);
        }
    for (int level = 0; level < 3; ++level) {
        float y = 1.6f + level * 2.9f;
        float t = y / (fy - 0.25f);
        float half = glm::mix(base, H + 0.6f, t);
        m->addPart("brace_h", box(vec3(2 * half, 0.07f, 0.07f), 0.80f), steel, T(0, y, -half), 0.3f);
        m->addPart("brace_h", box(vec3(2 * half, 0.07f, 0.07f), 0.80f), steel, T(0, y, half), 0.3f);
        m->addPart("brace_h", box(vec3(0.07f, 0.07f, 2 * half), 0.80f), steel, T(-half, y, 0), 0.3f);
        m->addPart("brace_h", box(vec3(0.07f, 0.07f, 2 * half), 0.80f), steel, T(half, y, 0), 0.3f);
        // diagonals on the north and south faces
        float y2 = y + 2.9f;
        float t2 = y2 / (fy - 0.25f);
        float half2 = glm::mix(base, H + 0.6f, std::min(1.f, t2));
        for (int s = -1; s <= 1; s += 2) {
            vec3 a(-half, y, s * half), b(half2, y2, s * half2);
            vec3 d = b - a;
            float len = glm::length(d);
            vec3 dir = d / len;
            vec3 axis = glm::cross(vec3(0, 1, 0), dir);
            float ang = std::acos(glm::clamp(dir.y, -1.f, 1.f));
            m->addPart("brace_d", box(vec3(0.05f, len, 0.05f), 0.80f), steel,
                       T(a.x, a.y, a.z) * glm::rotate(mat4(1.0f), ang, glm::normalize(axis)) * T(0, len * 0.5f, 0), 0.3f);
        }
    }
    // ladder on the south face
    for (int i = 0; i < 20; ++i) m->addPart("ladder_rung", box(vec3(0.5f, 0.03f, 0.03f), 0.80f), steel, T(0.0f, 0.5f + i * 0.46f, rr + 0.1f), 0.3f);
    m->addPart("ladder_rail", box(vec3(0.04f, 9.6f, 0.04f), 0.80f), steel, T(-0.25f, 5.0f, rr + 0.1f), 0.3f);
    m->addPart("ladder_rail", box(vec3(0.04f, 9.6f, 0.04f), 0.80f), steel, T(0.25f, 5.0f, rr + 0.1f), 0.3f);
    m->finalizeBounds();
    return m;
}

}  // namespace aegis
