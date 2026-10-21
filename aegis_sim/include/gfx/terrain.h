#pragma once
// Analytic terrain (height function), polar-grid mesh, roads, and prop placement.

#include <vector>

#include "core/scene.h"
#include "gfx/assets.h"
#include "gfx/mesh.h"

namespace aegis {

struct PropInstance {
    mat4 m{1.0f};
    vec3 pos{0.f};
    float maxScale = 1.0f;
};

class Terrain {
public:
    void init(TerrainType type, uint32_t seed);

    TerrainType type() const { return type_; }
    int typeIndex() const { return static_cast<int>(type_); }
    bool hasSea() const { return type_ == TerrainType::Coastal; }
    float seaLevel() const { return -6.0f; }
    vec2 coastDir() const { return coastDir_; }

    float height(float x, float z) const;
    vec3 normal(float x, float z, float e = 1.0f) const;
    // x = road mask, y = sidewalk mask, z = lane dash (mirrors shaders/terrain.frag)
    vec3 roadInfo(float x, float z) const;
    // Straight line visibility test against the terrain.
    bool lineOfSight(const vec3& a, const vec3& b, float clearance = 1.5f) const;

    void buildMeshes(Mesh& terrain, Mesh& water) const;
    void placeProps();

    std::vector<PropInstance> props[static_cast<int>(AssetId::Count)];
    std::vector<vec3> wireLines;  // pairs of points

private:
    void addProp(AssetId id, float x, float z, float yaw, vec3 scale, float sink = 0.0f);
    void placePoleLine(const std::vector<vec2>& pts, const std::vector<vec2>& axes);
    float ruralRoadX(float z) const;

    TerrainType type_ = TerrainType::Rural;
    uint32_t seed_ = 1;
    float ox_ = 0.f, oz_ = 0.f;
    vec2 coastDir_{0.f, 1.f};
};

}  // namespace aegis
