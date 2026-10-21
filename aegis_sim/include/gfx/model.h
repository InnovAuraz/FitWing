#pragma once
// Renderable model: a list of mesh parts (one per glTF primitive) with PBR materials.

#include <memory>
#include <string>
#include <vector>

#include "gfx/mesh.h"

namespace aegis {

struct Material {
    GLuint albedo = 0;    // sRGB
    GLuint normal = 0;    // linear
    GLuint orm = 0;       // linear: R=AO G=roughness B=metallic
    GLuint emissive = 0;  // sRGB
    vec4 baseColor{1.f};
    float metallic = 1.0f;   // multiplies ORM.b
    float roughness = 1.0f;  // multiplies ORM.g
    vec3 emissiveFactor{0.f};
    float normalStrength = 1.0f;
    float uvScale = 1.0f;
    bool alphaMask = false;
    float alphaCutoff = 0.5f;
    bool doubleSided = false;
    bool aoFromOrm = true;
    bool procedural = false;  // procedural material: factors blend with the texture instead of multiplying
};

struct ModelPart {
    std::string name;
    Mesh mesh;
    int material = 0;
    mat4 local{1.0f};   // relative to the model root
    float heat = 0.45f; // thermal emission 0..1 (used by thermal vision)
    int rotor = 0;      // 0 = static, +1/-1 = spins around its local Y axis
};

enum class Anchor { None, Center, GroundCenter };

struct ModelSpec {
    const char* file = "";
    float size = 1.0f;     // the largest bounding-box dimension is scaled to this many metres
    Anchor anchor = Anchor::None;
    float baseHeat = 0.45f;
};

class Model {
public:
    Model() = default;
    ~Model();
    Model(const Model&) = delete;
    Model& operator=(const Model&) = delete;

    std::string name;
    bool fromFile = false;
    std::vector<ModelPart> parts;
    std::vector<Material> mats;
    std::vector<GLuint> ownedTextures;
    vec3 bmin{0.f}, bmax{0.f};  // bounds of the final (scaled, anchored) model

    int addMaterial(const Material& m) {
        mats.push_back(m);
        return static_cast<int>(mats.size()) - 1;
    }
    ModelPart& addPart(const std::string& partName, const MeshData& data, int material, const mat4& local = mat4(1.0f),
                       float heat = 0.45f, int rotor = 0);
    void finalizeBounds();
    float radius() const { return glm::length(bmax - bmin) * 0.5f; }
    vec3 center() const { return (bmin + bmax) * 0.5f; }
};

// Loads a .glb/.gltf. Returns nullptr (and logs why) when the file is missing or invalid.
std::unique_ptr<Model> loadGLB(const std::string& path, const ModelSpec& spec);

}  // namespace aegis
