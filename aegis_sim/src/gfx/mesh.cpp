#include "gfx/mesh.h"

#include <cmath>

namespace aegis {

// ---------------------------------------------------------------------------------------------
// MeshData
// ---------------------------------------------------------------------------------------------
void MeshData::append(const MeshData& o, const mat4& m) {
    uint32_t base = static_cast<uint32_t>(v.size());
    mat3 nm = glm::transpose(glm::inverse(mat3(m)));
    mat3 tm = mat3(m);
    bool flip = glm::determinant(mat3(m)) < 0.0f;
    v.reserve(v.size() + o.v.size());
    for (const Vertex& s : o.v) {
        Vertex d = s;
        d.pos = vec3(m * vec4(s.pos, 1.0f));
        d.nrm = glm::normalize(nm * s.nrm);
        d.tan = vec4(glm::normalize(tm * vec3(s.tan)), flip ? -s.tan.w : s.tan.w);
        v.push_back(d);
    }
    i.reserve(i.size() + o.i.size());
    if (!flip) {
        for (uint32_t idx : o.i) i.push_back(base + idx);
    } else {  // mirrored transform: keep triangle winding front-facing
        for (size_t k = 0; k + 2 < o.i.size(); k += 3) {
            i.push_back(base + o.i[k]);
            i.push_back(base + o.i[k + 2]);
            i.push_back(base + o.i[k + 1]);
        }
    }
}

void MeshData::transform(const mat4& m) {
    MeshData t;
    t.append(*this, m);
    *this = std::move(t);
}

void MeshData::scaleUV(vec2 s) {
    for (auto& x : v) x.uv *= s;
}

void MeshData::computeSmoothNormals() {
    for (auto& x : v) x.nrm = vec3(0.f);
    for (size_t k = 0; k + 2 < i.size(); k += 3) {
        Vertex& a = v[i[k]];
        Vertex& b = v[i[k + 1]];
        Vertex& c = v[i[k + 2]];
        vec3 n = glm::cross(b.pos - a.pos, c.pos - a.pos);  // area weighted
        a.nrm += n;
        b.nrm += n;
        c.nrm += n;
    }
    for (auto& x : v) {
        float l = glm::length(x.nrm);
        x.nrm = l > 1e-12f ? x.nrm / l : vec3(0.f, 1.f, 0.f);
    }
}

void MeshData::computeTangents() {
    std::vector<vec3> tan1(v.size(), vec3(0.f)), tan2(v.size(), vec3(0.f));
    for (size_t k = 0; k + 2 < i.size(); k += 3) {
        uint32_t i0 = i[k], i1 = i[k + 1], i2 = i[k + 2];
        const Vertex &a = v[i0], &b = v[i1], &c = v[i2];
        vec3 e1 = b.pos - a.pos, e2 = c.pos - a.pos;
        vec2 d1 = b.uv - a.uv, d2 = c.uv - a.uv;
        float det = d1.x * d2.y - d2.x * d1.y;
        if (std::fabs(det) < 1e-20f) continue;
        float r = 1.0f / det;
        vec3 t = (e1 * d2.y - e2 * d1.y) * r;   // dp/du
        vec3 bv = (e2 * d1.x - e1 * d2.x) * r;  // dp/dv
        tan1[i0] += t; tan1[i1] += t; tan1[i2] += t;
        tan2[i0] += bv; tan2[i1] += bv; tan2[i2] += bv;
    }
    for (size_t k = 0; k < v.size(); ++k) {
        vec3 n = v[k].nrm;
        vec3 t = tan1[k];
        t = t - n * glm::dot(n, t);
        if (glm::dot(t, t) < 1e-18f) {
            // Degenerate UVs: pick any perpendicular vector.
            vec3 ref = std::fabs(n.y) < 0.99f ? vec3(0.f, 1.f, 0.f) : vec3(1.f, 0.f, 0.f);
            t = glm::cross(ref, n);
        }
        t = glm::normalize(t);
        vec3 bGreen = -tan2[k];  // green channel points towards decreasing v
        float w = glm::dot(glm::cross(n, t), bGreen) < 0.0f ? -1.0f : 1.0f;
        v[k].tan = vec4(t, w);
    }
}

void MeshData::bounds(vec3& mn, vec3& mx) const {
    mn = vec3(1e30f);
    mx = vec3(-1e30f);
    for (const auto& x : v) {
        mn = glm::min(mn, x.pos);
        mx = glm::max(mx, x.pos);
    }
    if (v.empty()) mn = mx = vec3(0.f);
}

// ---------------------------------------------------------------------------------------------
// Mesh (GPU)
// ---------------------------------------------------------------------------------------------
Mesh& Mesh::operator=(Mesh&& o) noexcept {
    if (this != &o) {
        destroy();
        vao_ = o.vao_; vbo_ = o.vbo_; ibo_ = o.ibo_;
        indexCount_ = o.indexCount_;
        bmin_ = o.bmin_; bmax_ = o.bmax_; radius_ = o.radius_;
        o.vao_ = o.vbo_ = o.ibo_ = 0;
        o.indexCount_ = 0;
    }
    return *this;
}

void Mesh::destroy() {
    if (vao_) glDeleteVertexArrays(1, &vao_);
    if (vbo_) glDeleteBuffers(1, &vbo_);
    if (ibo_) glDeleteBuffers(1, &ibo_);
    vao_ = vbo_ = ibo_ = 0;
    indexCount_ = 0;
}

void Mesh::upload(const MeshData& d, bool dynamic) {
    destroy();
    if (d.empty()) return;
    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &vbo_);
    glGenBuffers(1, &ibo_);
    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(d.v.size() * sizeof(Vertex)), d.v.data(),
                 dynamic ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo_);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(d.i.size() * sizeof(uint32_t)), d.i.data(),
                 dynamic ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW);
    const GLsizei stride = sizeof(Vertex);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(offsetof(Vertex, pos)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(offsetof(Vertex, nrm)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(offsetof(Vertex, uv)));
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(offsetof(Vertex, tan)));
    glBindVertexArray(0);
    indexCount_ = static_cast<int>(d.i.size());
    d.bounds(bmin_, bmax_);
    radius_ = glm::length(bmax_ - bmin_) * 0.5f;
}

void Mesh::draw() const {
    if (!vao_) return;
    glBindVertexArray(vao_);
    glDrawElements(GL_TRIANGLES, indexCount_, GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);
}

void Mesh::drawInstanced(GLuint instanceVbo, int count) const {
    if (!vao_ || count <= 0) return;
    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, instanceVbo);
    for (int c = 0; c < 4; ++c) {
        glEnableVertexAttribArray(4 + c);
        glVertexAttribPointer(4 + c, 4, GL_FLOAT, GL_FALSE, sizeof(mat4), reinterpret_cast<void*>(sizeof(vec4) * c));
        glVertexAttribDivisor(4 + c, 1);
    }
    glDrawElementsInstanced(GL_TRIANGLES, indexCount_, GL_UNSIGNED_INT, nullptr, count);
    for (int c = 0; c < 4; ++c) {
        glVertexAttribDivisor(4 + c, 0);
        glDisableVertexAttribArray(4 + c);
    }
    glBindVertexArray(0);
}

// ---------------------------------------------------------------------------------------------
// Builders
// ---------------------------------------------------------------------------------------------
namespace build {

static void addFace(MeshData& m, const vec3& origin, const vec3& du, const vec3& dv, const vec3& n, float uvScale) {
    // quad origin + s*du + t*dv, s,t in [0,1]. UV in metres.
    uint32_t b = static_cast<uint32_t>(m.v.size());
    float lu = glm::length(du) * uvScale, lv = glm::length(dv) * uvScale;
    const vec2 uvs[4] = {{0, 0}, {lu, 0}, {lu, lv}, {0, lv}};
    const vec3 ps[4] = {origin, origin + du, origin + du + dv, origin + dv};
    for (int k = 0; k < 4; ++k) {
        Vertex v;
        v.pos = ps[k];
        v.nrm = n;
        v.uv = uvs[k];
        m.v.push_back(v);
    }
    // winding so that (du x dv) agrees with n
    if (glm::dot(glm::cross(du, dv), n) > 0.0f) {
        m.i.insert(m.i.end(), {b, b + 1, b + 2, b, b + 2, b + 3});
    } else {
        m.i.insert(m.i.end(), {b, b + 2, b + 1, b, b + 3, b + 2});
    }
}

MeshData boxMinMax(const vec3& mn, const vec3& mx, float uvScale) {
    MeshData m;
    vec3 s = mx - mn;
    // +X
    addFace(m, vec3(mx.x, mn.y, mx.z), vec3(0, 0, -s.z), vec3(0, s.y, 0), vec3(1, 0, 0), uvScale);
    // -X
    addFace(m, vec3(mn.x, mn.y, mn.z), vec3(0, 0, s.z), vec3(0, s.y, 0), vec3(-1, 0, 0), uvScale);
    // +Y
    addFace(m, vec3(mn.x, mx.y, mx.z), vec3(s.x, 0, 0), vec3(0, 0, -s.z), vec3(0, 1, 0), uvScale);
    // -Y
    addFace(m, vec3(mn.x, mn.y, mn.z), vec3(s.x, 0, 0), vec3(0, 0, s.z), vec3(0, -1, 0), uvScale);
    // +Z
    addFace(m, vec3(mn.x, mn.y, mx.z), vec3(s.x, 0, 0), vec3(0, s.y, 0), vec3(0, 0, 1), uvScale);
    // -Z
    addFace(m, vec3(mx.x, mn.y, mn.z), vec3(-s.x, 0, 0), vec3(0, s.y, 0), vec3(0, 0, -1), uvScale);
    m.computeTangents();
    return m;
}

MeshData box(const vec3& size, float uvScale) { return boxMinMax(-size * 0.5f, size * 0.5f, uvScale); }

MeshData cylinderTapered(float r0, float r1, float height, int segments, bool smooth, float uvScale) {
    MeshData m;
    segments = std::max(3, segments);
    float slant = std::sqrt(height * height + (r0 - r1) * (r0 - r1));
    float ny = (r0 - r1) / std::max(1e-6f, slant);
    float nr = height / std::max(1e-6f, slant);
    float circ0 = kTwoPi * std::max(r0, r1);
    for (int s = 0; s < segments; ++s) {
        float a0 = kTwoPi * s / segments, a1 = kTwoPi * (s + 1) / segments;
        float am = 0.5f * (a0 + a1);
        vec3 d0(std::cos(a0), 0, std::sin(a0)), d1(std::cos(a1), 0, std::sin(a1)), dm(std::cos(am), 0, std::sin(am));
        uint32_t b = static_cast<uint32_t>(m.v.size());
        float u0 = circ0 * s / segments * uvScale, u1 = circ0 * (s + 1) / segments * uvScale;
        float vh = slant * uvScale;
        Vertex q[4];
        q[0].pos = d0 * r0;                       q[0].uv = {u0, 0};
        q[1].pos = d1 * r0;                       q[1].uv = {u1, 0};
        q[2].pos = d1 * r1 + vec3(0, height, 0);  q[2].uv = {u1, vh};
        q[3].pos = d0 * r1 + vec3(0, height, 0);  q[3].uv = {u0, vh};
        vec3 nm = glm::normalize(dm * nr + vec3(0, ny, 0));
        vec3 n0 = smooth ? glm::normalize(d0 * nr + vec3(0, ny, 0)) : nm;
        vec3 n1 = smooth ? glm::normalize(d1 * nr + vec3(0, ny, 0)) : nm;
        q[0].nrm = n0; q[1].nrm = n1; q[2].nrm = n1; q[3].nrm = n0;
        for (auto& x : q) m.v.push_back(x);
        // outward winding
        m.i.insert(m.i.end(), {b, b + 2, b + 1, b, b + 3, b + 2});
    }
    m.computeTangents();
    return m;
}

MeshData cylinder(float radius, float height, int segments, bool smooth, bool capTop, bool capBottom, float uvScale) {
    MeshData m = cylinderTapered(radius, radius, height, segments, smooth, uvScale);
    auto cap = [&](bool top) {
        uint32_t c = static_cast<uint32_t>(m.v.size());
        Vertex cv;
        cv.pos = vec3(0, top ? height : 0, 0);
        cv.nrm = vec3(0, top ? 1 : -1, 0);
        cv.uv = vec2(0.f);
        m.v.push_back(cv);
        for (int s = 0; s <= segments; ++s) {
            float a = kTwoPi * s / segments;
            Vertex v;
            v.pos = vec3(std::cos(a) * radius, top ? height : 0, std::sin(a) * radius);
            v.nrm = cv.nrm;
            v.uv = vec2(v.pos.x, v.pos.z) * uvScale;
            m.v.push_back(v);
        }
        for (int s = 0; s < segments; ++s) {
            if (top) m.i.insert(m.i.end(), {c, c + 2 + static_cast<uint32_t>(s), c + 1 + static_cast<uint32_t>(s)});
            else m.i.insert(m.i.end(), {c, c + 1 + static_cast<uint32_t>(s), c + 2 + static_cast<uint32_t>(s)});
        }
    };
    if (capTop) cap(true);
    if (capBottom) cap(false);
    m.computeTangents();
    return m;
}

MeshData cone(float radius, float height, int segments, float uvScale) {
    MeshData m = cylinderTapered(radius, 0.0f, height, segments, true, uvScale);
    // base cap
    uint32_t c = static_cast<uint32_t>(m.v.size());
    Vertex cv;
    cv.pos = vec3(0);
    cv.nrm = vec3(0, -1, 0);
    m.v.push_back(cv);
    for (int s = 0; s <= segments; ++s) {
        float a = kTwoPi * s / segments;
        Vertex v;
        v.pos = vec3(std::cos(a) * radius, 0, std::sin(a) * radius);
        v.nrm = cv.nrm;
        v.uv = vec2(v.pos.x, v.pos.z) * uvScale;
        m.v.push_back(v);
    }
    for (int s = 0; s < segments; ++s)
        m.i.insert(m.i.end(), {c, c + 1 + static_cast<uint32_t>(s), c + 2 + static_cast<uint32_t>(s)});
    m.computeTangents();
    return m;
}

MeshData sphere(float radius, int segments, int rings, float uvScale) {
    MeshData m;
    segments = std::max(3, segments);
    rings = std::max(2, rings);
    for (int r = 0; r <= rings; ++r) {
        float t = static_cast<float>(r) / rings;
        float phi = kPi * t;
        for (int s = 0; s <= segments; ++s) {
            float u = static_cast<float>(s) / segments;
            float th = kTwoPi * u;
            Vertex v;
            v.nrm = vec3(std::sin(phi) * std::cos(th), std::cos(phi), std::sin(phi) * std::sin(th));
            v.pos = v.nrm * radius;
            v.uv = vec2(u * kTwoPi * radius, t * kPi * radius) * uvScale;
            m.v.push_back(v);
        }
    }
    for (int r = 0; r < rings; ++r) {
        for (int s = 0; s < segments; ++s) {
            uint32_t a = static_cast<uint32_t>(r * (segments + 1) + s);
            uint32_t b = a + static_cast<uint32_t>(segments + 1);
            m.i.insert(m.i.end(), {a, a + 1, b, a + 1, b + 1, b});
        }
    }
    m.computeTangents();
    return m;
}

MeshData quad(float w, float h, bool facePlusZ, float uvScale) {
    MeshData m;
    vec3 n(0, 0, facePlusZ ? 1.f : -1.f);
    addFace(m, vec3(-w * 0.5f, -h * 0.5f, 0), vec3(facePlusZ ? w : -w, 0, 0), vec3(0, h, 0), n, uvScale);
    m.computeTangents();
    return m;
}

MeshData torus(float major, float minor, int segs, int sides, float uvScale) {
    MeshData m;
    for (int i = 0; i <= segs; ++i) {
        float a = kTwoPi * i / segs;
        vec3 c(std::cos(a) * major, 0, std::sin(a) * major);
        for (int j = 0; j <= sides; ++j) {
            float b = kTwoPi * j / sides;
            vec3 dir = vec3(std::cos(a) * std::cos(b), std::sin(b), std::sin(a) * std::cos(b));
            Vertex v;
            v.nrm = dir;
            v.pos = c + dir * minor;
            v.uv = vec2(a * major, b * minor) * uvScale;
            m.v.push_back(v);
        }
    }
    for (int i = 0; i < segs; ++i)
        for (int j = 0; j < sides; ++j) {
            uint32_t a = static_cast<uint32_t>(i * (sides + 1) + j), b = a + static_cast<uint32_t>(sides + 1);
            m.i.insert(m.i.end(), {a, a + 1, b, a + 1, b + 1, b});
        }
    m.computeTangents();
    return m;
}

}  // namespace build
}  // namespace aegis
