#pragma once
// Procedural PBR texture sets (albedo + normal + ORM) with optional PNG overrides from resources/.
//
// Override file names (all optional, any subset):  resources/tex_<kind>_albedo.png
//                                                 resources/tex_<kind>_normal.png
//                                                 resources/tex_<kind>_orm.png
// ORM = R: ambient occlusion, G: roughness, B: metallic.

#include <string>

#include "core/common.h"

namespace aegis {

enum class TexKind {
    Concrete, PaintedMetal, Wood, Asphalt, Grass, Dirt, Rock, Sand, Fabric, Water, Noise, Count
};

struct TexSet {
    GLuint albedo = 0;  // sRGB
    GLuint normal = 0;  // linear, OpenGL convention (green up)
    GLuint orm = 0;     // linear
};

const char* texKindName(TexKind k);

class TextureLibrary {
public:
    static TextureLibrary& get();
    void init(const std::string& resourceDir);  // needs a GL context
    void shutdown();
    const TexSet& set(TexKind k) const { return sets_[static_cast<int>(k)]; }

    GLuint white() const { return white_; }       // 1x1 white sRGB
    GLuint flatNormal() const { return flat_; }   // 1x1 (0.5,0.5,1)
    GLuint black() const { return black_; }       // 1x1 black
    GLuint whiteLinear() const { return whiteLin_; }

    // Loads an 8-bit image file (PNG/JPG) into a mip-mapped texture. Returns 0 on failure.
    static GLuint loadImageFile(const std::string& path, bool srgb);

private:
    TextureLibrary() = default;
    TexSet sets_[static_cast<int>(TexKind::Count)];
    GLuint white_ = 0, flat_ = 0, black_ = 0, whiteLin_ = 0;
    bool ready_ = false;
};

}  // namespace aegis
