#pragma once
// Immediate-mode 2D renderer (pixels, origin top-left) with a built-in 5x7 bitmap font.
// No font file is required.

#include <string>
#include <vector>

#include "core/common.h"
#include "gfx/gl_util.h"

namespace aegis {

enum class Align { Left, Center, Right };

class Ui {
public:
    bool init();
    void shutdown();
    void begin(int w, int h);
    void flush();

    void rect(float x, float y, float w, float h, const vec4& c);
    void rectGradV(float x, float y, float w, float h, const vec4& top, const vec4& bottom);
    void rectOutline(float x, float y, float w, float h, float t, const vec4& c);
    void line(float x0, float y0, float x1, float y1, float thickness, const vec4& c);
    void triangle(vec2 a, vec2 b, vec2 c, const vec4& col);
    void circle(float cx, float cy, float r, const vec4& c, int segs = 40);
    void ring(float cx, float cy, float r0, float r1, float a0, float a1, const vec4& c, int segs = 64);  // radians, 0 = up, clockwise
    void polyline(const std::vector<vec2>& pts, float thickness, const vec4& c);
    // Corner brackets  [  ]  around a rectangle (tactical look)
    void brackets(float x, float y, float w, float h, float len, float t, const vec4& c);

    void text(float x, float y, const std::string& s, float scale, const vec4& c, Align a = Align::Left);
    float textWidth(const std::string& s, float scale) const;
    float textHeight(float scale) const { return 7.0f * scale; }

    int width() const { return w_; }
    int height() const { return h_; }

private:
    struct V { float x, y, u, v; float r, g, b, a; };
    void quadSolid(float x0, float y0, float x1, float y1, const vec4& c0, const vec4& c1);
    std::vector<V> verts_;
    GLuint vao_ = 0, vbo_ = 0, font_ = 0;
    Shader shader_;
    int w_ = 1, h_ = 1;
};

}  // namespace aegis
