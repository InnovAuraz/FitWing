#include "gfx/terrain.h"

#include "gfx/noise.h"

namespace aegis {

namespace {
float posmod(float a, float b) {
    float r = std::fmod(a, b);
    return r < 0.0f ? r + b : r;
}
}  // namespace

void Terrain::init(TerrainType type, uint32_t seed) {
    type_ = type;
    seed_ = seed;
    Rng rng(seed * 2654435761u + 77u);
    ox_ = rng.uniform(-5000.f, 5000.f);
    oz_ = rng.uniform(-5000.f, 5000.f);
    float a = rng.uniform(0.0f, kTwoPi);
    coastDir_ = vec2(std::cos(a), std::sin(a));
}

float Terrain::ruralRoadX(float z) const { return 120.0f + 55.0f * std::sin(z / 310.0f) + 20.0f * std::sin(z / 97.0f + 1.0f); }

float Terrain::height(float x, float z) const {
    float r = std::sqrt(x * x + z * z);
    float flat = smoothstep(28.0f, 130.0f, r);
    float px = x + ox_, pz = z + oz_;
    int s = static_cast<int>(seed_ & 0xFFFF);
    float h = 0.0f;
    float d = x * coastDir_.x + z * coastDir_.y;
    switch (type_) {
        case TerrainType::Rural:
            h = (noise::fbm2(px / 520.f, pz / 520.f, 4, 2.0f, 0.5f, s) - 0.5f) * 2.0f * 13.0f +
                (noise::fbm2(px / 110.f, pz / 110.f, 3, 2.0f, 0.5f, s + 5) - 0.5f) * 3.0f;
            h *= flat;
            break;
        case TerrainType::Urban:
            h = (noise::fbm2(px / 900.f, pz / 900.f, 3, 2.0f, 0.5f, s) - 0.5f) * 2.0f * 3.0f * flat;
            break;
        case TerrainType::Forest:
            h = (noise::fbm2(px / 420.f, pz / 420.f, 4, 2.0f, 0.5f, s) - 0.5f) * 2.0f * 20.0f +
                (noise::fbm2(px / 85.f, pz / 85.f, 3, 2.0f, 0.5f, s + 5) - 0.5f) * 4.5f;
            h *= flat;
            break;
        case TerrainType::Coastal: {
            float land = ((noise::fbm2(px / 380.f, pz / 380.f, 4, 2.0f, 0.5f, s) - 0.5f) * 2.0f * 7.0f +
                          (noise::fbm2(px / 70.f, pz / 70.f, 3, 2.0f, 0.5f, s + 5) - 0.5f) * 1.6f) * flat;
            float sh = smoothstep(150.0f, 420.0f, d);
            h = land + (-14.0f - land) * sh;
            h += sh * (noise::fbm2(px / 200.f, pz / 200.f, 3, 2.0f, 0.5f, s + 9) - 0.5f) * 3.0f;
            break;
        }
    }
    float far = smoothstep(1900.0f, 4200.0f, r);
    if (far > 0.0f) {
        float m = noise::ridged2(px / 1500.f, pz / 1500.f, 4, s + 9) * 260.0f - 40.0f;
        if (type_ == TerrainType::Coastal) m *= 1.0f - smoothstep(-300.0f, 250.0f, d);
        if (type_ == TerrainType::Urban) m *= 0.5f;
        h += far * std::max(m, 0.0f);
    }
    return h;
}

vec3 Terrain::normal(float x, float z, float e) const {
    float hl = height(x - e, z), hr = height(x + e, z);
    float hd = height(x, z - e), hu = height(x, z + e);
    return glm::normalize(vec3(hl - hr, 2.0f * e, hd - hu));
}

vec3 Terrain::roadInfo(float x, float z) const {
    float m = 0.f, side = 0.f, dash = 0.f;
    switch (type_) {
        case TerrainType::Rural: m = 1.0f - smoothstep(2.6f, 3.8f, std::fabs(x - ruralRoadX(z))); break;
        case TerrainType::Urban: {
            float dx = std::fabs(posmod(x, 130.0f) - 65.0f);
            float dz = std::fabs(posmod(z, 130.0f) - 65.0f);
            float dr = std::min(dx, dz);
            m = 1.0f - smoothstep(6.0f, 7.2f, dr);
            side = smoothstep(6.0f, 7.2f, dr) * (1.0f - smoothstep(8.6f, 9.6f, dr));
            break;
        }
        case TerrainType::Forest: {
            float xc = 70.0f + 35.0f * std::sin(z / 180.0f) + 14.0f * std::sin(z / 61.0f + 2.0f);
            m = 1.0f - smoothstep(1.4f, 2.2f, std::fabs(x - xc));
            break;
        }
        case TerrainType::Coastal: {
            vec2 perp(-coastDir_.y, coastDir_.x);
            float s = x * perp.x + z * perp.y;
            float dd = std::fabs(x * coastDir_.x + z * coastDir_.y - 70.0f + 6.0f * std::sin(s / 140.0f));
            m = 1.0f - smoothstep(2.8f, 3.8f, dd);
            break;
        }
    }
    return vec3(m, side, dash);
}

bool Terrain::lineOfSight(const vec3& a, const vec3& b, float clearance) const {
    vec3 d = b - a;
    float len = glm::length(d);
    if (len < 1.0f) return true;
    int steps = std::min(400, std::max(2, static_cast<int>(len / 12.0f)));
    for (int i = 1; i < steps; ++i) {
        float t = static_cast<float>(i) / steps;
        vec3 p = a + d * t;
        if (p.y < height(p.x, p.z) + clearance) return false;
    }
    return true;
}

// ---------------------------------------------------------------------------------------------
void Terrain::buildMeshes(Mesh& terrain, Mesh& water) const {
    const int Nr = 230, Na = 512;
    const float r0 = 3.0f, R = 7000.0f;
    MeshData td, wd;
    td.v.reserve(static_cast<size_t>(Nr + 1) * Na);
    for (int i = 0; i <= Nr; ++i) {
        float r = r0 * std::pow(R / r0, static_cast<float>(i) / Nr);
        float e = std::min(25.0f, std::max(0.5f, r * 0.012f));
        for (int j = 0; j < Na; ++j) {
            float a = kTwoPi * j / Na;
            float x = r * std::cos(a), z = r * std::sin(a);
            Vertex v;
            float h = height(x, z);
            v.pos = vec3(x, h, z);
            v.nrm = normal(x, z, e);
            v.uv = vec2(x, z);
            td.v.push_back(v);
        }
    }
    for (int i = 0; i < Nr; ++i) {
        for (int j = 0; j < Na; ++j) {
            int j1 = (j + 1) % Na;
            uint32_t a = static_cast<uint32_t>(i * Na + j), b = static_cast<uint32_t>(i * Na + j1);
            uint32_t c = static_cast<uint32_t>((i + 1) * Na + j), d = static_cast<uint32_t>((i + 1) * Na + j1);
            td.i.insert(td.i.end(), {a, b, c, b, d, c});
        }
    }
    terrain.upload(td);
    if (hasSea()) {
        wd.v.reserve(td.v.size());
        for (const Vertex& s : td.v) {
            Vertex v;
            v.pos = vec3(s.pos.x, seaLevel(), s.pos.z);
            v.nrm = vec3(0, 1, 0);
            v.uv = vec2(seaLevel() - s.pos.y, 0.0f);
            wd.v.push_back(v);
        }
        // Keep only quads that touch water (any corner below sea level).
        for (int i = 0; i < Nr; ++i) {
            for (int j = 0; j < Na; ++j) {
                int j1 = (j + 1) % Na;
                uint32_t a = static_cast<uint32_t>(i * Na + j), b = static_cast<uint32_t>(i * Na + j1);
                uint32_t c = static_cast<uint32_t>((i + 1) * Na + j), d = static_cast<uint32_t>((i + 1) * Na + j1);
                float md = std::max(std::max(wd.v[a].uv.x, wd.v[b].uv.x), std::max(wd.v[c].uv.x, wd.v[d].uv.x));
                if (md < -0.2f) continue;
                wd.i.insert(wd.i.end(), {a, b, c, b, d, c});
            }
        }
        water.upload(wd);
    }
}

// ---------------------------------------------------------------------------------------------
void Terrain::addProp(AssetId id, float x, float z, float yaw, vec3 scale, float sink) {
    PropInstance p;
    float y = height(x, z) - sink;
    p.pos = vec3(x, y, z);
    p.m = glm::translate(mat4(1.f), p.pos) * glm::rotate(mat4(1.f), yaw, vec3(0, 1, 0)) * glm::scale(mat4(1.f), scale);
    p.maxScale = std::max(scale.x, std::max(scale.y, scale.z));
    props[static_cast<int>(id)].push_back(p);
}

void Terrain::placePoleLine(const std::vector<vec2>& pts, const std::vector<vec2>& axes) {
    for (size_t k = 0; k < pts.size(); ++k) {
        float yaw = std::atan2(-axes[k].y, axes[k].x);
        addProp(AssetId::UtilityPole, pts[k].x, pts[k].y, yaw, vec3(1.f));
    }
    for (size_t k = 0; k + 1 < pts.size(); ++k) {
        float h0 = height(pts[k].x, pts[k].y), h1 = height(pts[k + 1].x, pts[k + 1].y);
        for (int w = -1; w <= 1; ++w) {
            float yo = (w == 0) ? 9.35f : 9.35f;
            vec3 a(pts[k].x + axes[k].x * w, h0 + yo, pts[k].y + axes[k].y * w);
            vec3 b(pts[k + 1].x + axes[k + 1].x * w, h1 + yo, pts[k + 1].y + axes[k + 1].y * w);
            const int segs = 8;
            vec3 prev = a;
            for (int s = 1; s <= segs; ++s) {
                float t = static_cast<float>(s) / segs;
                vec3 p = a + (b - a) * t;
                p.y -= 4.0f * 0.9f * t * (1.0f - t);  // catenary-ish sag (0.9 m)
                wireLines.push_back(prev);
                wireLines.push_back(p);
                prev = p;
            }
        }
    }
}

void Terrain::placeProps() {
    for (auto& v : props) v.clear();
    wireLines.clear();
    Rng rng(seed_ * 7919u + 13u);
    int s = static_cast<int>(seed_ & 0xFFFF);
    auto road = [&](float x, float z) {
        vec3 ri = roadInfo(x, z);
        return std::max(ri.x, ri.y);
    };
    auto polar = [&](float rmin, float rmax, float& x, float& z) {
        float r = rmin + (rmax - rmin) * std::sqrt(rng.uniform());
        float a = rng.uniform(0.0f, kTwoPi);
        x = r * std::cos(a);
        z = r * std::sin(a);
    };
    auto slopeOk = [&](float x, float z, float maxSlope) { return 1.0f - normal(x, z, 2.0f).y < maxSlope; };
    auto landOk = [&](float x, float z) { return !hasSea() || height(x, z) > seaLevel() + 2.2f; };

    switch (type_) {
        case TerrainType::Rural: {
            for (int c = 0; c < 15; ++c) {
                float cx, cz;
                polar(100.f, 900.f, cx, cz);
                int n = rng.range(8, 26);
                for (int k = 0; k < n; ++k) {
                    float x = cx + rng.gauss() * 28.f, z = cz + rng.gauss() * 28.f;
                    if (std::sqrt(x * x + z * z) < 35.f || road(x, z) > 0.01f || !slopeOk(x, z, 0.4f)) continue;
                    bool pine = rng.chance(0.15f);
                    float sc = rng.uniform(0.75f, 1.3f);
                    addProp(pine ? AssetId::TreePine : AssetId::TreeBroadleaf, x, z, rng.uniform(0.f, kTwoPi), vec3(sc, sc * rng.uniform(0.9f, 1.15f), sc));
                }
            }
            for (int k = 0; k < 55; ++k) {
                float x, z;
                polar(40.f, 1100.f, x, z);
                if (road(x, z) > 0.01f) continue;
                float sc = rng.uniform(0.8f, 1.25f);
                addProp(AssetId::TreeBroadleaf, x, z, rng.uniform(0.f, kTwoPi), vec3(sc));
            }
            for (int k = 0; k < 2; ++k) {
                float x, z;
                polar(170.f, 380.f, x, z);
                if (road(x, z) > 0.01f) { --k; continue; }
                addProp(AssetId::Barn, x, z, rng.uniform(0.f, kTwoPi), vec3(1.f));
            }
            for (int k = 0; k < 6; ++k) {
                float x, z;
                polar(110.f, 520.f, x, z);
                if (road(x, z) > 0.01f) { --k; continue; }
                addProp(AssetId::HouseSmall, x, z, rng.uniform(0.f, kTwoPi), vec3(rng.uniform(0.9f, 1.15f)));
            }
            std::vector<vec2> pts, axes;
            for (float z = -1100.f; z <= 1100.f; z += 65.f) {
                pts.push_back(vec2(ruralRoadX(z) + 7.5f, z));
                axes.push_back(vec2(1.f, 0.f));
            }
            placePoleLine(pts, axes);
            break;
        }
        case TerrainType::Urban: {
            for (int iz = -7; iz <= 7; ++iz) {
                for (int ix = -7; ix <= 7; ++ix) {
                    if (ix == 0 && iz == 0) continue;
                    float cx = ix * 130.f, cz = iz * 130.f;
                    float dist = std::sqrt(cx * cx + cz * cz);
                    if (dist > 980.f) continue;
                    for (int a = -1; a <= 1; a += 2)
                        for (int b = -1; b <= 1; b += 2) {
                            if (!rng.chance(0.8f)) continue;
                            float x = cx + a * 27.f + rng.uniform(-4.f, 4.f), z = cz + b * 27.f + rng.uniform(-4.f, 4.f);
                            float yaw = rng.range(0, 3) * kPi * 0.5f;
                            if (rng.chance(dist < 450.f ? 0.62f : 0.5f)) {
                                float sy = dist < 330.f ? rng.uniform(0.34f, 0.58f) : rng.uniform(0.45f, 0.95f);
                                addProp(AssetId::ApartmentBlock, x, z, yaw, vec3(rng.uniform(0.7f, 1.0f), sy, rng.uniform(0.7f, 1.0f)));
                            } else {
                                addProp(AssetId::HouseSmall, x, z, yaw, vec3(rng.uniform(0.95f, 1.25f)));
                            }
                        }
                    int trees = rng.range(0, 3);
                    for (int t = 0; t < trees; ++t) {
                        float x = cx + rng.uniform(-45.f, 45.f), z = cz + rng.uniform(-45.f, 45.f);
                        if (road(x, z) > 0.01f) continue;
                        float sc = rng.uniform(0.7f, 1.1f);
                        addProp(AssetId::TreeBroadleaf, x, z, rng.uniform(0.f, kTwoPi), vec3(sc));
                    }
                }
            }
            // street poles along the x-roads closest to the post
            std::vector<vec2> pts, axes;
            for (float x = -520.f; x <= 520.f; x += 60.f) {
                if (std::fabs(x) < 40.f) continue;
                pts.push_back(vec2(x, 65.f - 10.f));
                axes.push_back(vec2(0.f, 1.f));
            }
            // two segments (left and right of the post) so wires do not cross the plaza
            std::vector<vec2> left, right, la, ra;
            for (size_t k = 0; k < pts.size(); ++k) {
                if (pts[k].x < 0) { left.push_back(pts[k]); la.push_back(axes[k]); }
                else { right.push_back(pts[k]); ra.push_back(axes[k]); }
            }
            placePoleLine(left, la);
            placePoleLine(right, ra);
            break;
        }
        case TerrainType::Forest: {
            int pines = 0, broad = 0;
            for (int k = 0; k < 9000 && (pines < 2000 || broad < 520); ++k) {
                float x, z;
                polar(30.f, 1050.f, x, z);
                float r = std::sqrt(x * x + z * z);
                float dens = noise::fbm2((x + ox_) / 150.f, (z + oz_) / 150.f, 3, 2.0f, 0.5f, s + 21);
                if (dens < 0.36f) continue;
                if (!rng.chance(smoothstep(35.f, 175.f, r))) continue;
                if (road(x, z) > 0.01f || !slopeOk(x, z, 0.45f)) continue;
                bool pine = rng.chance(0.78f);
                if ((pine && pines >= 2000) || (!pine && broad >= 520)) continue;
                float sc = rng.uniform(0.7f, 1.35f);
                addProp(pine ? AssetId::TreePine : AssetId::TreeBroadleaf, x, z, rng.uniform(0.f, kTwoPi), vec3(sc, sc * rng.uniform(0.9f, 1.2f), sc));
                (pine ? pines : broad)++;
            }
            for (int k = 0; k < 3; ++k) {
                float x, z;
                polar(120.f, 420.f, x, z);
                if (road(x, z) > 0.01f) { --k; continue; }
                addProp(AssetId::HouseSmall, x, z, rng.uniform(0.f, kTwoPi), vec3(0.85f));
            }
            break;
        }
        case TerrainType::Coastal: {
            vec2 cd = coastDir_, perp(-cd.y, cd.x);
            // houses between the post and the road
            for (int k = 0; k < 8; ++k) {
                float sPos = rng.uniform(-520.f, 520.f);
                float d = rng.uniform(-60.f, 25.f);
                vec2 p = cd * d + perp * sPos;
                if (std::sqrt(p.x * p.x + p.y * p.y) < 70.f || !landOk(p.x, p.y)) { --k; continue; }
                float yaw = std::atan2(-perp.y, perp.x) + rng.uniform(-0.15f, 0.15f);
                addProp(AssetId::HouseSmall, p.x, p.y, yaw, vec3(rng.uniform(0.9f, 1.15f)));
            }
            // lighthouse on the headland
            for (int k = 0; k < 60; ++k) {
                float sPos = rng.uniform(450.f, 760.f) * (rng.chance(0.5f) ? 1.f : -1.f);
                float d = rng.uniform(120.f, 260.f);
                vec2 p = cd * d + perp * sPos;
                if (height(p.x, p.y) > seaLevel() + 3.0f) {
                    addProp(AssetId::Lighthouse, p.x, p.y, rng.uniform(0.f, kTwoPi), vec3(1.f));
                    break;
                }
            }
            int rocks = 0;
            for (int k = 0; k < 2500 && rocks < 55; ++k) {
                float x, z;
                polar(60.f, 1200.f, x, z);
                float h = height(x, z);
                if (h < seaLevel() - 0.3f || h > seaLevel() + 3.5f) continue;
                float sc = rng.uniform(0.6f, 1.8f);
                addProp(AssetId::RockCluster, x, z, rng.uniform(0.f, kTwoPi), vec3(sc, sc * rng.uniform(0.7f, 1.2f), sc), 0.4f);
                ++rocks;
            }
            for (int k = 0; k < 75; ++k) {
                float x, z;
                polar(55.f, 800.f, x, z);
                float d = x * cd.x + z * cd.y;
                if (d > 40.f || road(x, z) > 0.01f || !landOk(x, z)) continue;
                float sc = rng.uniform(0.7f, 1.2f);
                addProp(rng.chance(0.65f) ? AssetId::TreePine : AssetId::TreeBroadleaf, x, z, rng.uniform(0.f, kTwoPi), vec3(sc));
            }
            std::vector<vec2> pts, axes;
            for (float sPos = -900.f; sPos <= 900.f; sPos += 65.f) {
                float d = 70.f - 6.0f * std::sin(sPos / 140.0f) - 8.f;
                pts.push_back(cd * d + perp * sPos);
                axes.push_back(cd);
            }
            placePoleLine(pts, axes);
            break;
        }
    }
    size_t total = 0;
    for (auto& v : props) total += v.size();
    LOG_INFO("terrain '%s': %zu props, %zu wire segments", toString(type_), total, wireLines.size() / 2);
}

}  // namespace aegis
