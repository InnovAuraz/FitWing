#pragma once
// Named asset catalogue.  Every entry maps to a .glb file in resources/.  If the file is absent
// or invalid, a built-in procedural model with the same proportions is used, so the simulator
// always runs and you can swap in real models one at a time by only changing resources/.

#include <memory>
#include <string>

#include "gfx/model.h"

namespace aegis {

enum class AssetId {
    Drone, Rifle, Launcher, Missile,
    TreePine, TreeBroadleaf, HouseSmall, ApartmentBlock, Barn, UtilityPole, Lighthouse, RockCluster,
    Count
};

struct AssetInfo {
    const char* file;
    ModelSpec spec;
};

const ModelSpec& assetSpec(AssetId id);

class AssetCatalog {
public:
    void load(const std::string& resourceDir);
    const Model& get(AssetId id) const { return *models_[static_cast<int>(id)]; }
    bool fromFile(AssetId id) const { return models_[static_cast<int>(id)]->fromFile; }

private:
    std::unique_ptr<Model> models_[static_cast<int>(AssetId::Count)];
};

// Built-in procedural fallbacks (also exposed for the cab/props code).
std::unique_ptr<Model> buildFallbackModel(AssetId id);
Material makeMaterial(int texKind, vec3 tint, float roughness, float metallic, float uvScale);

}  // namespace aegis
