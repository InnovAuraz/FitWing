#pragma once
// CPU mesh data, GPU mesh, and a few procedural builders (box, cylinder, cone, sphere...).
//
// Vertex layout (shader locations): 0 pos, 1 normal, 2 uv, 3 tangent(xyz) + handedness(w).
// Instanced draws additionally bind a mat4 per instance at locations 4..7.
//
// Tangent convention (identical for glTF models and procedural meshes):
//   T = d(pos)/du,   B_green = -d(pos)/dv   (green channel = "up" in the image, image row 0 = v 0)
//   bitangent used by shaders = cross(N, T) * tangent.w

#include <vector>

#include "core/common.h"

namespace aegis {

struct Vertex {
    vec3 pos{0.f};
    vec3 nrm{0.f, 1.f, 0.f};
    vec2 uv{0.f};
    vec4 tan{1.f, 0.f, 0.f, 1.f};
};

struct MeshData {
    std::vector<Vertex> v;
    std::vector<uint32_t> i;

    bool empty() const { return v.empty() || i.empty(); }
    void clear() {
        v.clear();
        i.clear();
    }
    // Appends another mesh transformed by m (normals/tangents use the upper 3x3 inverse-transpose).
    void append(const MeshData& o, const mat4& m = mat4(1.0f));
    void computeSmoothNormals();
    void computeTangents();
    void bounds(vec3& mn, vec3& mx) const;
    void transform(const mat4& m);
    void scaleUV(vec2 s);
};

class Mesh {
public:
    Mesh() = default;
    ~Mesh() { destroy(); }
    Mesh(const Mesh&) = delete;
    Mesh& operator=(const Mesh&) = delete;
    Mesh(Mesh&& o) noexcept { *this = std::move(o); }
    Mesh& operator=(Mesh&& o) noexcept;

    void upload(const MeshData& d, bool dynamic = false);
    void destroy();
    bool valid() const { return vao_ != 0; }

    void draw() const;
    void drawInstanced(GLuint instanceVbo, int count) const;

    vec3 boundsMin() const { return bmin_; }
    vec3 boundsMax() const { return bmax_; }
    float boundsRadius() const { return radius_; }
    int indexCount() const { return indexCount_; }

private:
    GLuint vao_ = 0, vbo_ = 0, ibo_ = 0;
    int indexCount_ = 0;
    vec3 bmin_{0.f}, bmax_{0.f};
    float radius_ = 0.f;
};

namespace build {
// All builders emit UVs in *metres* multiplied by uvScale so textures keep a constant texel density
// regardless of object size. Tangents are generated.
MeshData box(const vec3& size, float uvScale = 1.0f);                          // centred on origin
MeshData boxMinMax(const vec3& mn, const vec3& mx, float uvScale = 1.0f);
MeshData cylinder(float radius, float height, int segments, bool smooth = true, bool capTop = true,
                  bool capBottom = true, float uvScale = 1.0f);                // axis +Y, base at y=0
MeshData cone(float radius, float height, int segments, float uvScale = 1.0f);  // base at y=0
MeshData sphere(float radius, int segments, int rings, float uvScale = 1.0f);
MeshData quad(float width, float height, bool facePlusZ = true, float uvScale = 1.0f);  // centred XY plane
MeshData cylinderTapered(float r0, float r1, float height, int segments, bool smooth = true, float uvScale = 1.0f);
MeshData torus(float major, float minor, int segs, int sides, float uvScale = 1.0f);
}  // namespace build

}  // namespace aegis
