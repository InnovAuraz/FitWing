// tinygltf implementation lives here (single translation unit).  stb_image's implementation is
// compiled here as well, so other files may call stbi_load() after including stb_image.h.
#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define TINYGLTF_NO_STB_IMAGE_WRITE
#include "tiny_gltf.h"

#include "gfx/model.h"

#include <cstring>

#include "core/paths.h"
#include "gfx/gl_util.h"
#include "gfx/textures.h"

namespace aegis {

Model::~Model() {
    if (!ownedTextures.empty()) glDeleteTextures(static_cast<GLsizei>(ownedTextures.size()), ownedTextures.data());
}

ModelPart& Model::addPart(const std::string& partName, const MeshData& data, int material, const mat4& local, float heat,
                          int rotor) {
    parts.emplace_back();
    ModelPart& p = parts.back();
    p.name = partName;
    p.mesh.upload(data);
    p.material = material;
    p.local = local;
    p.heat = heat;
    p.rotor = rotor;
    return p;
}

void Model::finalizeBounds() {
    vec3 mn(1e30f), mx(-1e30f);
    for (const auto& p : parts) {
        if (!p.mesh.valid()) continue;
        vec3 a = p.mesh.boundsMin(), b = p.mesh.boundsMax();
        for (int c = 0; c < 8; ++c) {
            vec3 q((c & 1) ? b.x : a.x, (c & 2) ? b.y : a.y, (c & 4) ? b.z : a.z);
            vec3 w = vec3(p.local * vec4(q, 1.0f));
            mn = glm::min(mn, w);
            mx = glm::max(mx, w);
        }
    }
    if (mn.x > mx.x) mn = mx = vec3(0.f);
    bmin = mn;
    bmax = mx;
}

// ---------------------------------------------------------------------------------------------
namespace {

using tinygltf::Accessor;

const unsigned char* accessorPtr(const tinygltf::Model& m, const Accessor& a, size_t& stride) {
    const auto& bv = m.bufferViews[static_cast<size_t>(a.bufferView)];
    const auto& buf = m.buffers[static_cast<size_t>(bv.buffer)];
    int comps = tinygltf::GetNumComponentsInType(static_cast<uint32_t>(a.type));
    int csz = tinygltf::GetComponentSizeInBytes(static_cast<uint32_t>(a.componentType));
    stride = a.ByteStride(bv) > 0 ? static_cast<size_t>(a.ByteStride(bv)) : static_cast<size_t>(comps * csz);
    return buf.data.data() + bv.byteOffset + a.byteOffset;
}

float readComponent(const unsigned char* p, int ctype, bool normalized) {
    switch (ctype) {
        case TINYGLTF_COMPONENT_TYPE_FLOAT: {
            float f;
            std::memcpy(&f, p, 4);
            return f;
        }
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE: return normalized ? *p / 255.0f : static_cast<float>(*p);
        case TINYGLTF_COMPONENT_TYPE_BYTE: {
            int8_t v = static_cast<int8_t>(*p);
            return normalized ? std::max(v / 127.0f, -1.0f) : static_cast<float>(v);
        }
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT: {
            uint16_t v;
            std::memcpy(&v, p, 2);
            return normalized ? v / 65535.0f : static_cast<float>(v);
        }
        case TINYGLTF_COMPONENT_TYPE_SHORT: {
            int16_t v;
            std::memcpy(&v, p, 2);
            return normalized ? std::max(v / 32767.0f, -1.0f) : static_cast<float>(v);
        }
        default: return 0.0f;
    }
}

bool readFloats(const tinygltf::Model& m, int accIdx, int wantComps, std::vector<float>& out) {
    if (accIdx < 0 || accIdx >= static_cast<int>(m.accessors.size())) return false;
    const Accessor& a = m.accessors[static_cast<size_t>(accIdx)];
    if (a.bufferView < 0) return false;
    int comps = tinygltf::GetNumComponentsInType(static_cast<uint32_t>(a.type));
    int csz = tinygltf::GetComponentSizeInBytes(static_cast<uint32_t>(a.componentType));
    size_t stride;
    const unsigned char* base = accessorPtr(m, a, stride);
    out.assign(a.count * static_cast<size_t>(wantComps), 0.0f);
    for (size_t i = 0; i < a.count; ++i) {
        for (int c = 0; c < wantComps; ++c) {
            if (c < comps) out[i * wantComps + c] = readComponent(base + i * stride + static_cast<size_t>(c * csz), a.componentType, a.normalized);
        }
    }
    return true;
}

bool readIndices(const tinygltf::Model& m, int accIdx, std::vector<uint32_t>& out) {
    if (accIdx < 0) return false;
    const Accessor& a = m.accessors[static_cast<size_t>(accIdx)];
    if (a.bufferView < 0) return false;
    size_t stride;
    const unsigned char* base = accessorPtr(m, a, stride);
    out.resize(a.count);
    for (size_t i = 0; i < a.count; ++i) {
        const unsigned char* p = base + i * stride;
        switch (a.componentType) {
            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE: out[i] = *p; break;
            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT: {
                uint16_t v;
                std::memcpy(&v, p, 2);
                out[i] = v;
                break;
            }
            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT: {
                uint32_t v;
                std::memcpy(&v, p, 4);
                out[i] = v;
                break;
            }
            default: return false;
        }
    }
    return true;
}

mat4 nodeLocal(const tinygltf::Node& n) {
    if (n.matrix.size() == 16) {
        mat4 m;
        for (int i = 0; i < 16; ++i) glm::value_ptr(m)[i] = static_cast<float>(n.matrix[static_cast<size_t>(i)]);
        return m;
    }
    vec3 t(0.f), s(1.f);
    glm::quat q(1.f, 0.f, 0.f, 0.f);
    if (n.translation.size() == 3) t = vec3(n.translation[0], n.translation[1], n.translation[2]);
    if (n.scale.size() == 3) s = vec3(n.scale[0], n.scale[1], n.scale[2]);
    if (n.rotation.size() == 4) q = glm::quat(static_cast<float>(n.rotation[3]), static_cast<float>(n.rotation[0]),
                                              static_cast<float>(n.rotation[1]), static_cast<float>(n.rotation[2]));
    return glm::translate(mat4(1.f), t) * glm::mat4_cast(q) * glm::scale(mat4(1.f), s);
}

GLuint uploadImage(const tinygltf::Image& img, bool srgb) {
    if (img.image.empty() || img.width <= 0 || img.height <= 0) return 0;
    int comp = img.component > 0 ? img.component : 4;
    int bytesPer = img.bits == 16 ? 2 : 1;
    size_t n = static_cast<size_t>(img.width) * static_cast<size_t>(img.height);
    std::vector<uint8_t> rgba(n * 4, 255);
    for (size_t i = 0; i < n; ++i) {
        uint8_t px[4] = {0, 0, 0, 255};
        for (int c = 0; c < comp && c < 4; ++c) {
            size_t o = (i * static_cast<size_t>(comp) + static_cast<size_t>(c)) * static_cast<size_t>(bytesPer);
            px[c] = img.image[o + (bytesPer == 2 ? 1 : 0)];
        }
        if (comp == 1) px[1] = px[2] = px[0];
        if (comp == 2) { px[3] = px[1]; px[1] = px[2] = px[0]; }
        rgba[i * 4 + 0] = px[0]; rgba[i * 4 + 1] = px[1]; rgba[i * 4 + 2] = px[2]; rgba[i * 4 + 3] = px[3];
    }
    GLuint t = createTexture2D(img.width, img.height, srgb ? GL_SRGB8_ALPHA8 : GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE,
                               rgba.data(), true);
    setAnisotropy(t, 8.0f);
    return t;
}

}  // namespace

std::unique_ptr<Model> loadGLB(const std::string& path, const ModelSpec& spec) {
    if (!fileExists(path)) return nullptr;
    tinygltf::TinyGLTF loader;
    tinygltf::Model gm;
    std::string err, warn;
    bool ok;
    if (path.size() > 5 && path.compare(path.size() - 5, 5, ".gltf") == 0) ok = loader.LoadASCIIFromFile(&gm, &err, &warn, path);
    else ok = loader.LoadBinaryFromFile(&gm, &err, &warn, path);
    if (!warn.empty()) LOG_WARN("%s: %s", path.c_str(), warn.c_str());
    if (!ok) {
        LOG_ERROR("failed to load %s: %s", path.c_str(), err.c_str());
        return nullptr;
    }

    auto model = std::make_unique<Model>();
    model->name = spec.file;
    model->fromFile = true;

    // textures (cache per image index + colour space)
    std::vector<GLuint> texSrgb(gm.images.size(), 0), texLin(gm.images.size(), 0);
    auto getTex = [&](int texIndex, bool srgb) -> GLuint {
        if (texIndex < 0 || texIndex >= static_cast<int>(gm.textures.size())) return 0;
        int src = gm.textures[static_cast<size_t>(texIndex)].source;
        if (src < 0 || src >= static_cast<int>(gm.images.size())) return 0;
        GLuint& slot = srgb ? texSrgb[static_cast<size_t>(src)] : texLin[static_cast<size_t>(src)];
        if (!slot) {
            slot = uploadImage(gm.images[static_cast<size_t>(src)], srgb);
            if (slot) model->ownedTextures.push_back(slot);
        }
        return slot;
    };

    auto& lib = TextureLibrary::get();
    // materials
    for (const auto& gmat : gm.materials) {
        Material m;
        const auto& pbr = gmat.pbrMetallicRoughness;
        m.baseColor = vec4(static_cast<float>(pbr.baseColorFactor[0]), static_cast<float>(pbr.baseColorFactor[1]),
                           static_cast<float>(pbr.baseColorFactor[2]), static_cast<float>(pbr.baseColorFactor[3]));
        m.metallic = static_cast<float>(pbr.metallicFactor);
        m.roughness = static_cast<float>(pbr.roughnessFactor);
        m.albedo = getTex(pbr.baseColorTexture.index, true);
        m.normal = getTex(gmat.normalTexture.index, false);
        m.normalStrength = gmat.normalTexture.index >= 0 ? static_cast<float>(gmat.normalTexture.scale) : 1.0f;
        m.orm = getTex(pbr.metallicRoughnessTexture.index, false);
        m.aoFromOrm = gmat.occlusionTexture.index >= 0 && gmat.occlusionTexture.index == pbr.metallicRoughnessTexture.index;
        m.emissive = getTex(gmat.emissiveTexture.index, true);
        m.emissiveFactor = vec3(static_cast<float>(gmat.emissiveFactor[0]), static_cast<float>(gmat.emissiveFactor[1]),
                                static_cast<float>(gmat.emissiveFactor[2]));
        if (m.emissive && glm::dot(m.emissiveFactor, m.emissiveFactor) < 1e-6f) m.emissiveFactor = vec3(1.f);
        if (gmat.alphaMode == "MASK") {
            m.alphaMask = true;
            m.alphaCutoff = static_cast<float>(gmat.alphaCutoff);
        } else if (gmat.alphaMode == "BLEND") {
            m.alphaMask = true;
            m.alphaCutoff = 0.4f;
        }
        m.doubleSided = gmat.doubleSided;
        if (!m.albedo) m.albedo = lib.white();
        if (!m.normal) m.normal = lib.flatNormal();
        if (!m.orm) {
            m.orm = lib.whiteLinear();
            m.aoFromOrm = false;
        }
        if (!m.emissive) m.emissive = lib.black();
        model->addMaterial(m);
    }
    {
        Material def;  // used for primitives without a material
        def.albedo = lib.white();
        def.normal = lib.flatNormal();
        def.orm = lib.whiteLinear();
        def.emissive = lib.black();
        def.baseColor = vec4(0.6f, 0.6f, 0.6f, 1.f);
        def.metallic = 0.f;
        def.roughness = 0.8f;
        def.aoFromOrm = false;
        model->addMaterial(def);
    }
    const int defaultMat = static_cast<int>(model->mats.size()) - 1;

    // walk nodes
    int sceneIdx = gm.defaultScene >= 0 ? gm.defaultScene : 0;
    std::vector<int> roots;
    if (!gm.scenes.empty()) roots = gm.scenes[static_cast<size_t>(std::min<int>(sceneIdx, static_cast<int>(gm.scenes.size()) - 1))].nodes;
    else
        for (size_t i = 0; i < gm.nodes.size(); ++i) roots.push_back(static_cast<int>(i));

    std::vector<ModelPart> collected;
    std::vector<mat4> localMats;
    std::vector<MeshData> datas;
    std::vector<std::string> names;
    std::vector<int> mats;

    struct Item { int node; mat4 parent; };
    std::vector<Item> stack;
    for (int r : roots) stack.push_back({r, mat4(1.f)});
    int guard = 0;
    while (!stack.empty() && guard++ < 100000) {
        Item it = stack.back();
        stack.pop_back();
        if (it.node < 0 || it.node >= static_cast<int>(gm.nodes.size())) continue;
        const tinygltf::Node& node = gm.nodes[static_cast<size_t>(it.node)];
        mat4 world = it.parent * nodeLocal(node);
        for (int c : node.children) stack.push_back({c, world});
        if (node.mesh < 0 || node.mesh >= static_cast<int>(gm.meshes.size())) continue;
        const tinygltf::Mesh& gmesh = gm.meshes[static_cast<size_t>(node.mesh)];
        for (const auto& prim : gmesh.primitives) {
            if (prim.mode != TINYGLTF_MODE_TRIANGLES && prim.mode != -1) continue;
            auto pit = prim.attributes.find("POSITION");
            if (pit == prim.attributes.end()) continue;
            std::vector<float> pos, nrm, uv, tan;
            if (!readFloats(gm, pit->second, 3, pos)) continue;
            size_t vc = pos.size() / 3;
            bool hasN = false, hasT = false;
            auto nit = prim.attributes.find("NORMAL");
            if (nit != prim.attributes.end()) hasN = readFloats(gm, nit->second, 3, nrm);
            auto uit = prim.attributes.find("TEXCOORD_0");
            if (uit != prim.attributes.end()) readFloats(gm, uit->second, 2, uv);
            auto tit = prim.attributes.find("TANGENT");
            if (tit != prim.attributes.end()) hasT = readFloats(gm, tit->second, 4, tan);
            MeshData md;
            md.v.resize(vc);
            for (size_t i = 0; i < vc; ++i) {
                md.v[i].pos = vec3(pos[i * 3], pos[i * 3 + 1], pos[i * 3 + 2]);
                if (hasN) md.v[i].nrm = glm::normalize(vec3(nrm[i * 3], nrm[i * 3 + 1], nrm[i * 3 + 2]));
                if (uv.size() >= (i + 1) * 2) md.v[i].uv = vec2(uv[i * 2], uv[i * 2 + 1]);
                if (hasT) md.v[i].tan = vec4(tan[i * 4], tan[i * 4 + 1], tan[i * 4 + 2], tan[i * 4 + 3]);
            }
            if (!readIndices(gm, prim.indices, md.i)) {
                md.i.resize(vc);
                for (size_t i = 0; i < vc; ++i) md.i[i] = static_cast<uint32_t>(i);
            }
            md.i.resize(md.i.size() / 3 * 3);
            bool bad = false;
            for (uint32_t ix : md.i)
                if (ix >= vc) bad = true;
            if (bad) {
                LOG_WARN("%s: primitive with out-of-range indices skipped", spec.file);
                continue;
            }
            if (!hasN) md.computeSmoothNormals();
            if (!hasT) md.computeTangents();
            datas.push_back(std::move(md));
            localMats.push_back(world);
            names.push_back(node.name);
            mats.push_back(prim.material >= 0 && prim.material < static_cast<int>(gm.materials.size()) ? prim.material : defaultMat);
        }
    }
    if (datas.empty()) {
        LOG_ERROR("%s contains no triangle meshes", path.c_str());
        return nullptr;
    }

    // bounds in model space
    vec3 mn(1e30f), mx(-1e30f);
    for (size_t k = 0; k < datas.size(); ++k) {
        vec3 a, b;
        datas[k].bounds(a, b);
        for (int c = 0; c < 8; ++c) {
            vec3 q((c & 1) ? b.x : a.x, (c & 2) ? b.y : a.y, (c & 4) ? b.z : a.z);
            vec3 w = vec3(localMats[k] * vec4(q, 1.f));
            mn = glm::min(mn, w);
            mx = glm::max(mx, w);
        }
    }
    vec3 ext = mx - mn;
    float maxDim = std::max(ext.x, std::max(ext.y, ext.z));
    float s = maxDim > 1e-6f ? spec.size / maxDim : 1.0f;
    vec3 off(0.f);
    if (spec.anchor == Anchor::Center) off = -(mn + mx) * 0.5f * s;
    else if (spec.anchor == Anchor::GroundCenter) off = vec3(-(mn.x + mx.x) * 0.5f * s, -mn.y * s, -(mn.z + mx.z) * 0.5f * s);
    mat4 root = glm::translate(mat4(1.f), off) * glm::scale(mat4(1.f), vec3(s));

    for (size_t k = 0; k < datas.size(); ++k) {
        std::string nm = names[k];
        std::string low = nm;
        for (auto& ch : low) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        float heat = spec.baseHeat;
        int rotor = 0;
        if (low.rfind("rotor", 0) == 0 || low.rfind("prop", 0) == 0) {
            rotor = (k % 2 == 0) ? 1 : -1;
            // alternate directions by the numeric/letter suffix to mimic counter-rotating props
            int sum = 0;
            for (char ch : low) sum += static_cast<unsigned char>(ch);
            rotor = (sum % 2 == 0) ? 1 : -1;
            heat = 0.62f;
        } else if (low.rfind("motor", 0) == 0 || low.rfind("battery", 0) == 0 || low.rfind("engine", 0) == 0) {
            heat = 1.0f;
        }
        ModelPart& part = model->addPart(nm.empty() ? "part" : nm, datas[k], mats[k], root * localMats[k], heat, rotor);
        (void)part;
    }
    model->finalizeBounds();
    LOG_INFO("loaded %s (%zu parts, scaled x%.3f to %.2f m)", spec.file, model->parts.size(), s, spec.size);
    return model;
}

}  // namespace aegis
