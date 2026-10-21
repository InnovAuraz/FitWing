#include "gfx/textures.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <functional>
#include <thread>

#include "core/paths.h"
#include "gfx/gl_util.h"
#include "gfx/noise.h"

#include "stb_image.h"  // implementation lives in model.cpp (via tinygltf)

namespace aegis {

using namespace noise;

const char* texKindName(TexKind k) {
    switch (k) {
        case TexKind::Concrete: return "concrete";
        case TexKind::PaintedMetal: return "metal";
        case TexKind::Wood: return "wood";
        case TexKind::Asphalt: return "asphalt";
        case TexKind::Grass: return "grass";
        case TexKind::Dirt: return "dirt";
        case TexKind::Rock: return "rock";
        case TexKind::Sand: return "sand";
        case TexKind::Fabric: return "fabric";
        case TexKind::Water: return "water";
        case TexKind::Noise: return "noise";
        default: return "?";
    }
}

namespace {

struct Pix {
    vec3 albedo{0.5f};  // sRGB-ish display colour 0..1
    float height = 0.5f;
    float ao = 1.0f;
    float rough = 0.8f;
    float metal = 0.0f;
};

struct Images {
    int n = 0;
    std::vector<uint8_t> albedo, normal, orm;
};

void parallelRows(int rows, const std::function<void(int)>& fn) {
    unsigned hw = std::max(1u, std::thread::hardware_concurrency());
    unsigned nt = std::min<unsigned>(hw, 8u);
    std::vector<std::thread> th;
    std::atomic<int> next{0};
    for (unsigned t = 0; t < nt; ++t) {
        th.emplace_back([&]() {
            for (;;) {
                int r = next.fetch_add(1);
                if (r >= rows) break;
                fn(r);
            }
        });
    }
    for (auto& x : th) x.join();
}

inline float sat(float x) { return std::min(1.f, std::max(0.f, x)); }
inline vec3 mix3(vec3 a, vec3 b, float t) { return a + (b - a) * sat(t); }
inline float sstep(float a, float b, float x) {
    float t = sat((x - a) / (b - a));
    return t * t * (3 - 2 * t);
}

// ---- per-kind pixel functions (u,v in [0,1)) -----------------------------------------------
Pix pixConcrete(float u, float v) {
    Pix p;
    float big = fbmTile(u, v, 4, 5, 11);
    float fine = fbmTile(u, v, 64, 3, 12);
    float pits = cellTile(u, v, 48, 13);
    float pit = 1.0f - sstep(0.06f, 0.16f, pits);
    float streak = fbmTile(u, v * 0.25f, 6, 4, 14);  // v-stretched => vertical stains
    float crack = ridgedTile(u, v, 5, 4, 15);
    float crackMask = sstep(0.80f, 0.93f, crack);
    float h = 0.55f * big + 0.35f * fine - 0.35f * pit - 0.25f * crackMask;
    float g = 0.36f + 0.20f * big + 0.06f * fine;
    g *= 1.0f - 0.35f * sstep(0.55f, 0.9f, streak);
    g *= 1.0f - 0.45f * pit - 0.5f * crackMask;
    p.albedo = vec3(g * 1.02f, g, g * 0.95f);
    p.height = h;
    p.ao = 1.0f - 0.6f * pit - 0.5f * crackMask;
    p.rough = 0.82f + 0.12f * fine;
    return p;
}

Pix pixPaintedMetal(float u, float v) {
    Pix p;
    // 4x4 panels
    float fu = u * 4.0f, fv = v * 4.0f;
    float cu = fu - std::floor(fu), cv = fv - std::floor(fv);
    float edge = std::min(std::min(cu, 1 - cu), std::min(cv, 1 - cv));
    float seam = 1.0f - sstep(0.0f, 0.02f, edge);
    float bevel = sstep(0.0f, 0.06f, edge);
    // rivets
    float rv = 1e9f;
    for (int k = 0; k < 4; ++k) {
        float rx = (k & 1) ? 0.88f : 0.12f, ry = (k & 2) ? 0.88f : 0.12f;
        float d = std::sqrt((cu - rx) * (cu - rx) + (cv - ry) * (cv - ry));
        rv = std::min(rv, d);
    }
    float rivet = 1.0f - sstep(0.018f, 0.032f, rv);
    float blotch = fbmTile(u, v, 6, 5, 21);
    float fine = fbmTile(u, v, 80, 2, 22);
    float scratchN = ridgedTile(u * 1.0f, v * 0.12f, 24, 2, 23);
    float scratch = sstep(0.88f, 0.97f, scratchN);
    float rustN = fbmTile(u, v, 5, 5, 24) + 0.35f * (1.0f - bevel) + 0.18f * seam;
    float rust = sstep(0.72f, 0.95f, rustN);
    vec3 paint = mix3(vec3(0.17f, 0.22f, 0.13f), vec3(0.23f, 0.28f, 0.17f), blotch);
    paint *= 0.9f + 0.2f * fine;
    vec3 rustc = mix3(vec3(0.36f, 0.16f, 0.07f), vec3(0.50f, 0.26f, 0.10f), fine);
    vec3 steel(0.55f, 0.56f, 0.57f);
    vec3 c = mix3(paint, rustc, rust);
    c = mix3(c, steel, scratch * (1.0f - rust));
    c *= 1.0f - 0.6f * seam;
    p.albedo = c;
    p.height = 0.5f + 0.18f * bevel + 0.5f * rivet - 0.6f * seam + 0.06f * fine + 0.1f * rust * fine;
    p.ao = 1.0f - 0.7f * seam;
    p.rough = mix3(vec3(0.42f), vec3(0.9f), rust).x + 0.1f * fine;
    p.metal = sat(scratch * 1.0f * (1.0f - rust) + 0.05f);
    return p;
}

Pix pixWood(float u, float v) {
    Pix p;
    // planks run along u; 6 planks across v
    float planks = 6.0f;
    float pv = v * planks;
    int pi = static_cast<int>(std::floor(pv));
    float pf = pv - std::floor(pv);
    float rnd = hash2(pi, 3, 31);
    float warp = fbmTile(u, v, 4, 3, 32 + pi);
    float grain = std::sin((pf * 38.0f + warp * 14.0f + rnd * 20.0f) * 1.0f);
    float fib = fbmTile(u * 0.15f + rnd, v * 1.0f, 64, 2, 33);
    float knotC = cellTile(u, v, 5, 34);
    float knot = 1.0f - sstep(0.05f, 0.2f, knotC);
    float gap = 1.0f - sstep(0.0f, 0.05f, std::min(pf, 1.0f - pf));
    vec3 base = mix3(vec3(0.30f, 0.18f, 0.09f), vec3(0.46f, 0.29f, 0.15f), rnd * 0.8f + 0.2f * fib);
    float g = 0.5f + 0.5f * grain;
    vec3 c = base * (0.78f + 0.3f * g) * (1.0f - 0.35f * knot);
    c *= 1.0f - 0.8f * gap;
    p.albedo = c;
    p.height = 0.5f + 0.06f * g + 0.1f * fib - 0.5f * gap - 0.08f * knot;
    p.ao = 1.0f - 0.8f * gap;
    p.rough = 0.62f + 0.18f * fib;
    return p;
}

Pix pixAsphalt(float u, float v) {
    Pix p;
    float f = fbmTile(u, v, 8, 4, 41);
    float grit = cellTile(u, v, 90, 42);
    float stone = 1.0f - sstep(0.1f, 0.4f, grit);
    float tone = hash2(static_cast<int>(u * 90), static_cast<int>(v * 90), 43);
    float g = 0.075f + 0.05f * f + 0.05f * stone * tone;
    float crack = sstep(0.86f, 0.95f, ridgedTile(u, v, 4, 4, 44));
    g *= 1.0f - 0.6f * crack;
    p.albedo = vec3(g, g, g * 1.04f);
    p.height = 0.4f * f + 0.5f * stone * (0.5f + tone) - 0.5f * crack;
    p.ao = 1.0f - 0.5f * crack;
    p.rough = 0.88f - 0.2f * stone * tone;
    return p;
}

Pix pixGrass(float u, float v) {
    Pix p;
    float patch = fbmTile(u, v, 5, 5, 51);
    float blades = fbmTile(u, v, 96, 3, 52);
    float blades2 = valueTile(u, v * 0.4f, 160, 53);
    float clump = 1.0f - cellTile(u, v, 28, 54);
    vec3 lush(0.13f, 0.27f, 0.055f), dry(0.30f, 0.30f, 0.11f), dark(0.06f, 0.14f, 0.035f);
    vec3 c = mix3(lush, dry, sstep(0.45f, 0.8f, patch));
    c = mix3(c, dark, sstep(0.55f, 0.95f, 1.0f - clump) * 0.7f);
    c *= 0.65f + 0.7f * blades;
    c += vec3(0.02f, 0.05f, 0.0f) * blades2;
    p.albedo = c;
    p.height = 0.55f * blades + 0.35f * blades2 + 0.3f * clump;
    p.ao = 0.55f + 0.45f * sat(blades + 0.2f);
    p.rough = 0.92f;
    return p;
}

Pix pixDirt(float u, float v) {
    Pix p;
    float f = fbmTile(u, v, 6, 5, 61);
    float clod = cellTile(u, v, 24, 62);
    float peb = cellTile(u, v, 70, 63);
    float pebble = 1.0f - sstep(0.12f, 0.25f, peb);
    float tone = hash2(static_cast<int>(u * 70), static_cast<int>(v * 70), 64);
    vec3 c = mix3(vec3(0.20f, 0.13f, 0.075f), vec3(0.34f, 0.24f, 0.14f), f);
    c *= 0.8f + 0.35f * clod;
    c = mix3(c, vec3(0.42f, 0.40f, 0.36f) * (0.6f + 0.5f * tone), pebble * 0.8f);
    p.albedo = c;
    p.height = 0.5f * f + 0.4f * clod + 0.5f * pebble;
    p.ao = 0.7f + 0.3f * clod;
    p.rough = 0.95f;
    return p;
}

Pix pixRock(float u, float v) {
    Pix p;
    float r = ridgedTile(u, v, 4, 6, 71);
    float f = fbmTile(u, v, 10, 5, 72);
    float cell = cellTile(u, v, 6, 73);
    float crack = sstep(0.82f, 0.96f, ridgedTile(u, v, 5, 4, 74));
    float strata = 0.5f + 0.5f * std::sin((v + 0.08f * f) * 6.2831853f * 9.0f);
    vec3 c = mix3(vec3(0.20f, 0.19f, 0.18f), vec3(0.42f, 0.40f, 0.36f), f);
    c = mix3(c, vec3(0.32f, 0.27f, 0.22f), sstep(0.5f, 0.9f, strata) * 0.3f);
    c *= 0.7f + 0.5f * r;
    c *= 1.0f - 0.7f * crack;
    p.albedo = c;
    p.height = 0.7f * r + 0.3f * f + 0.15f * cell - 0.6f * crack;
    p.ao = 1.0f - 0.7f * crack;
    p.rough = 0.88f;
    return p;
}

Pix pixSand(float u, float v) {
    Pix p;
    float w = fbmTile(u, v, 4, 4, 81);
    float ripple = 0.5f + 0.5f * std::sin((u * 0.6f + v + w * 0.35f) * 6.2831853f * 8.0f);
    float grain = hash2(static_cast<int>(u * 256), static_cast<int>(v * 256), 82);
    vec3 c = mix3(vec3(0.46f, 0.38f, 0.24f), vec3(0.64f, 0.55f, 0.36f), w);
    c *= 0.88f + 0.22f * ripple;
    c *= 0.92f + 0.16f * grain;
    p.albedo = c;
    p.height = 0.4f * ripple + 0.3f * w + 0.06f * grain;
    p.ao = 0.8f + 0.2f * ripple;
    p.rough = 0.95f;
    return p;
}

Pix pixFabric(float u, float v) {
    Pix p;
    float th = 64.0f;
    float a = 0.5f + 0.5f * std::sin(u * 6.2831853f * th);
    float b = 0.5f + 0.5f * std::sin(v * 6.2831853f * th);
    int cu = static_cast<int>(std::floor(u * th)), cv = static_cast<int>(std::floor(v * th));
    float over = ((cu + cv) & 1) ? a : b;
    float f = fbmTile(u, v, 8, 4, 91);
    float dirt = fbmTile(u, v, 5, 5, 92);
    vec3 c = mix3(vec3(0.19f, 0.21f, 0.12f), vec3(0.30f, 0.30f, 0.18f), f);
    c *= 0.75f + 0.35f * over;
    c = mix3(c, vec3(0.28f, 0.22f, 0.15f), sstep(0.6f, 0.95f, dirt) * 0.5f);
    p.albedo = c;
    p.height = 0.5f * over + 0.2f * f;
    p.ao = 0.7f + 0.3f * over;
    p.rough = 0.95f;
    return p;
}

Pix pixWater(float u, float v) {
    Pix p;
    float a = fbmTile(u, v, 6, 5, 101);
    float b = ridgedTile(u * 1.0f, v * 1.0f, 9, 4, 102);
    p.albedo = vec3(0.0f);
    p.height = 0.6f * a + 0.4f * b;
    p.rough = 0.05f;
    return p;
}

Pix pixNoise(float u, float v) {  // channels are filled specially in generate()
    Pix p;
    p.height = fbmTile(u, v, 4, 6, 111);
    return p;
}

void generate(TexKind kind, int n, Images& img, float normalStrength) {
    img.n = n;
    img.albedo.assign(static_cast<size_t>(n) * n * 4, 255);
    img.normal.assign(static_cast<size_t>(n) * n * 4, 255);
    img.orm.assign(static_cast<size_t>(n) * n * 4, 255);
    std::vector<float> H(static_cast<size_t>(n) * n);
    std::vector<Pix> P(static_cast<size_t>(n) * n);
    auto fn = [&](float u, float v) -> Pix {
        switch (kind) {
            case TexKind::Concrete: return pixConcrete(u, v);
            case TexKind::PaintedMetal: return pixPaintedMetal(u, v);
            case TexKind::Wood: return pixWood(u, v);
            case TexKind::Asphalt: return pixAsphalt(u, v);
            case TexKind::Grass: return pixGrass(u, v);
            case TexKind::Dirt: return pixDirt(u, v);
            case TexKind::Rock: return pixRock(u, v);
            case TexKind::Sand: return pixSand(u, v);
            case TexKind::Fabric: return pixFabric(u, v);
            case TexKind::Water: return pixWater(u, v);
            default: return pixNoise(u, v);
        }
    };
    parallelRows(n, [&](int y) {
        for (int x = 0; x < n; ++x) {
            float u = (x + 0.5f) / n, v = (y + 0.5f) / n;
            P[static_cast<size_t>(y) * n + x] = fn(u, v);
            H[static_cast<size_t>(y) * n + x] = P[static_cast<size_t>(y) * n + x].height;
        }
    });
    auto hAt = [&](int x, int y) { return H[static_cast<size_t>((y + n) % n) * n + ((x + n) % n)]; };
    parallelRows(n, [&](int y) {
        for (int x = 0; x < n; ++x) {
            size_t k = (static_cast<size_t>(y) * n + x) * 4;
            const Pix& p = P[static_cast<size_t>(y) * n + x];
            if (kind == TexKind::Noise) {
                float u = (x + 0.5f) / n, v = (y + 0.5f) / n;
                float c0 = fbmTile(u, v, 3, 6, 111), c1 = fbmTile(u, v, 7, 5, 112);
                float c2 = fbmTile(u, v, 17, 4, 113), c3 = fbmTile(u, v, 41, 3, 114);
                img.albedo[k] = static_cast<uint8_t>(sat(c0) * 255);
                img.albedo[k + 1] = static_cast<uint8_t>(sat(c1) * 255);
                img.albedo[k + 2] = static_cast<uint8_t>(sat(c2) * 255);
                img.albedo[k + 3] = static_cast<uint8_t>(sat(c3) * 255);
                continue;
            }
            // Sobel on height
            float tl = hAt(x - 1, y - 1), t = hAt(x, y - 1), tr = hAt(x + 1, y - 1);
            float l = hAt(x - 1, y), r = hAt(x + 1, y);
            float bl = hAt(x - 1, y + 1), b = hAt(x, y + 1), br = hAt(x + 1, y + 1);
            float dx = (tr + 2 * r + br) - (tl + 2 * l + bl);
            float dy = (bl + 2 * b + br) - (tl + 2 * t + tr);  // y grows downwards in the image
            float s = normalStrength * n / 256.0f;
            vec3 nn = glm::normalize(vec3(-dx * s, dy * s, 1.0f));  // green = up in image
            img.normal[k] = static_cast<uint8_t>((nn.x * 0.5f + 0.5f) * 255);
            img.normal[k + 1] = static_cast<uint8_t>((nn.y * 0.5f + 0.5f) * 255);
            img.normal[k + 2] = static_cast<uint8_t>((nn.z * 0.5f + 0.5f) * 255);
            img.normal[k + 3] = 255;
            img.albedo[k] = static_cast<uint8_t>(sat(p.albedo.x) * 255);
            img.albedo[k + 1] = static_cast<uint8_t>(sat(p.albedo.y) * 255);
            img.albedo[k + 2] = static_cast<uint8_t>(sat(p.albedo.z) * 255);
            img.albedo[k + 3] = 255;
            img.orm[k] = static_cast<uint8_t>(sat(p.ao) * 255);
            img.orm[k + 1] = static_cast<uint8_t>(sat(p.rough) * 255);
            img.orm[k + 2] = static_cast<uint8_t>(sat(p.metal) * 255);
            img.orm[k + 3] = 255;
        }
    });
}

GLuint upload(const std::vector<uint8_t>& px, int n, bool srgb) {
    GLuint t = createTexture2D(n, n, srgb ? GL_SRGB8_ALPHA8 : GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, px.data(), true);
    setAnisotropy(t, 8.0f);
    return t;
}

GLuint solid(uint8_t r, uint8_t g, uint8_t b, bool srgb) {
    uint8_t px[4] = {r, g, b, 255};
    return createTexture2D(1, 1, srgb ? GL_SRGB8_ALPHA8 : GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, px, false, GL_NEAREST,
                           GL_NEAREST, GL_REPEAT);
}

}  // namespace

TextureLibrary& TextureLibrary::get() {
    static TextureLibrary inst;
    return inst;
}

GLuint TextureLibrary::loadImageFile(const std::string& path, bool srgb) {
    if (!fileExists(path)) return 0;
    int w = 0, h = 0, comp = 0;
    stbi_uc* data = stbi_load(path.c_str(), &w, &h, &comp, 4);
    if (!data) {
        LOG_WARN("could not decode image %s", path.c_str());
        return 0;
    }
    GLuint t = createTexture2D(w, h, srgb ? GL_SRGB8_ALPHA8 : GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, data, true);
    setAnisotropy(t, 8.0f);
    stbi_image_free(data);
    return t;
}

void TextureLibrary::init(const std::string& resourceDir) {
    if (ready_) return;
    white_ = solid(255, 255, 255, true);
    whiteLin_ = solid(255, 255, 255, false);
    flat_ = solid(128, 128, 255, false);
    black_ = solid(0, 0, 0, true);

    struct Spec { TexKind k; int size; float strength; };
    const Spec specs[] = {
        {TexKind::Concrete, 512, 3.0f},  {TexKind::PaintedMetal, 512, 2.5f}, {TexKind::Wood, 512, 2.0f},
        {TexKind::Asphalt, 512, 3.0f},   {TexKind::Grass, 512, 3.5f},        {TexKind::Dirt, 512, 3.0f},
        {TexKind::Rock, 512, 4.5f},      {TexKind::Sand, 512, 2.0f},         {TexKind::Fabric, 256, 2.5f},
        {TexKind::Water, 256, 4.0f},     {TexKind::Noise, 256, 1.0f},
    };
    for (const Spec& s : specs) {
        TexSet& out = sets_[static_cast<int>(s.k)];
        Images img;
        generate(s.k, s.size, img, s.strength);
        out.albedo = upload(img.albedo, s.size, s.k != TexKind::Noise);
        out.normal = upload(img.normal, s.size, false);
        out.orm = upload(img.orm, s.size, false);
        // Optional overrides
        std::string base = resourceDir + "/tex_" + texKindName(s.k);
        if (GLuint t = loadImageFile(base + "_albedo.png", true)) {
            glDeleteTextures(1, &out.albedo);
            out.albedo = t;
            LOG_INFO("texture override: tex_%s_albedo.png", texKindName(s.k));
        }
        if (GLuint t = loadImageFile(base + "_normal.png", false)) {
            glDeleteTextures(1, &out.normal);
            out.normal = t;
            LOG_INFO("texture override: tex_%s_normal.png", texKindName(s.k));
        }
        if (GLuint t = loadImageFile(base + "_orm.png", false)) {
            glDeleteTextures(1, &out.orm);
            out.orm = t;
            LOG_INFO("texture override: tex_%s_orm.png", texKindName(s.k));
        }
    }
    ready_ = true;
}

void TextureLibrary::shutdown() {
    for (auto& s : sets_) {
        GLuint t[3] = {s.albedo, s.normal, s.orm};
        glDeleteTextures(3, t);
        s = TexSet();
    }
    GLuint d[4] = {white_, flat_, black_, whiteLin_};
    glDeleteTextures(4, d);
    white_ = flat_ = black_ = whiteLin_ = 0;
    ready_ = false;
}

}  // namespace aegis
