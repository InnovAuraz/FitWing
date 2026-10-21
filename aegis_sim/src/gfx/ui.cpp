#include "gfx/ui.h"

#include <cstring>

namespace aegis {

// Classic 5x7 font, ASCII 32..126, 5 column bytes per glyph (bit0 = top row).
static const unsigned char kFont5x7[95][5] = {
    {0x00,0x00,0x00,0x00,0x00},{0x00,0x00,0x5F,0x00,0x00},{0x00,0x07,0x00,0x07,0x00},{0x14,0x7F,0x14,0x7F,0x14},
    {0x24,0x2A,0x7F,0x2A,0x12},{0x23,0x13,0x08,0x64,0x62},{0x36,0x49,0x56,0x20,0x50},{0x00,0x08,0x07,0x03,0x00},
    {0x00,0x1C,0x22,0x41,0x00},{0x00,0x41,0x22,0x1C,0x00},{0x2A,0x1C,0x7F,0x1C,0x2A},{0x08,0x08,0x3E,0x08,0x08},
    {0x00,0x80,0x70,0x30,0x00},{0x08,0x08,0x08,0x08,0x08},{0x00,0x00,0x60,0x60,0x00},{0x20,0x10,0x08,0x04,0x02},
    {0x3E,0x51,0x49,0x45,0x3E},{0x00,0x42,0x7F,0x40,0x00},{0x72,0x49,0x49,0x49,0x46},{0x21,0x41,0x49,0x4D,0x33},
    {0x18,0x14,0x12,0x7F,0x10},{0x27,0x45,0x45,0x45,0x39},{0x3C,0x4A,0x49,0x49,0x31},{0x41,0x21,0x11,0x09,0x07},
    {0x36,0x49,0x49,0x49,0x36},{0x46,0x49,0x49,0x29,0x1E},{0x00,0x00,0x14,0x00,0x00},{0x00,0x40,0x34,0x00,0x00},
    {0x00,0x08,0x14,0x22,0x41},{0x14,0x14,0x14,0x14,0x14},{0x00,0x41,0x22,0x14,0x08},{0x02,0x01,0x59,0x09,0x06},
    {0x3E,0x41,0x5D,0x59,0x4E},{0x7C,0x12,0x11,0x12,0x7C},{0x7F,0x49,0x49,0x49,0x36},{0x3E,0x41,0x41,0x41,0x22},
    {0x7F,0x41,0x41,0x41,0x3E},{0x7F,0x49,0x49,0x49,0x41},{0x7F,0x09,0x09,0x09,0x01},{0x3E,0x41,0x41,0x51,0x73},
    {0x7F,0x08,0x08,0x08,0x7F},{0x00,0x41,0x7F,0x41,0x00},{0x20,0x40,0x41,0x3F,0x01},{0x7F,0x08,0x14,0x22,0x41},
    {0x7F,0x40,0x40,0x40,0x40},{0x7F,0x02,0x1C,0x02,0x7F},{0x7F,0x04,0x08,0x10,0x7F},{0x3E,0x41,0x41,0x41,0x3E},
    {0x7F,0x09,0x09,0x09,0x06},{0x3E,0x41,0x51,0x21,0x5E},{0x7F,0x09,0x19,0x29,0x46},{0x26,0x49,0x49,0x49,0x32},
    {0x03,0x01,0x7F,0x01,0x03},{0x3F,0x40,0x40,0x40,0x3F},{0x1F,0x20,0x40,0x20,0x1F},{0x3F,0x40,0x38,0x40,0x3F},
    {0x63,0x14,0x08,0x14,0x63},{0x03,0x04,0x78,0x04,0x03},{0x61,0x59,0x49,0x4D,0x43},{0x00,0x7F,0x41,0x41,0x41},
    {0x02,0x04,0x08,0x10,0x20},{0x00,0x41,0x41,0x41,0x7F},{0x04,0x02,0x01,0x02,0x04},{0x40,0x40,0x40,0x40,0x40},
    {0x00,0x03,0x07,0x08,0x00},{0x20,0x54,0x54,0x78,0x40},{0x7F,0x28,0x44,0x44,0x38},{0x38,0x44,0x44,0x44,0x28},
    {0x38,0x44,0x44,0x28,0x7F},{0x38,0x54,0x54,0x54,0x18},{0x00,0x08,0x7E,0x09,0x02},{0x18,0xA4,0xA4,0x9C,0x78},
    {0x7F,0x08,0x04,0x04,0x78},{0x00,0x44,0x7D,0x40,0x00},{0x20,0x40,0x40,0x3D,0x00},{0x7F,0x10,0x28,0x44,0x00},
    {0x00,0x41,0x7F,0x40,0x00},{0x7C,0x04,0x78,0x04,0x78},{0x7C,0x08,0x04,0x04,0x78},{0x38,0x44,0x44,0x44,0x38},
    {0xFC,0x18,0x24,0x24,0x18},{0x18,0x24,0x24,0x18,0xFC},{0x7C,0x08,0x04,0x04,0x08},{0x48,0x54,0x54,0x54,0x24},
    {0x04,0x04,0x3F,0x44,0x24},{0x3C,0x40,0x40,0x20,0x7C},{0x1C,0x20,0x40,0x20,0x1C},{0x3C,0x40,0x30,0x40,0x3C},
    {0x44,0x28,0x10,0x28,0x44},{0x4C,0x90,0x90,0x90,0x7C},{0x44,0x64,0x54,0x4C,0x44},{0x00,0x08,0x36,0x41,0x00},
    {0x00,0x00,0x77,0x00,0x00},{0x00,0x41,0x36,0x08,0x00},{0x02,0x01,0x02,0x04,0x02}};

static const int kCell = 8;      // atlas cell size in pixels
static const int kCols = 16;
static const int kRows = 6;

bool Ui::init() {
    // font atlas (R8): 16 x 6 cells of 8x8, glyph occupies columns 1..5 / rows 0..6
    std::vector<unsigned char> atlas(static_cast<size_t>(kCols * kCell) * kRows * kCell, 0);
    const int aw = kCols * kCell;
    for (int g = 0; g < 95; ++g) {
        int cx = (g % kCols) * kCell, cy = (g / kCols) * kCell;
        for (int col = 0; col < 5; ++col) {
            unsigned char bits = kFont5x7[g][col];
            for (int row = 0; row < 7; ++row) {
                if (bits & (1 << row)) atlas[static_cast<size_t>(cy + row) * aw + (cx + 1 + col)] = 255;
            }
        }
    }
    glGenTextures(1, &font_);
    glBindTexture(GL_TEXTURE_2D, font_);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, aw, kRows * kCell, 0, GL_RED, GL_UNSIGNED_BYTE, atlas.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &vbo_);
    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, 1 << 20, nullptr, GL_STREAM_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(V), reinterpret_cast<void*>(offsetof(V, x)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(V), reinterpret_cast<void*>(offsetof(V, u)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(V), reinterpret_cast<void*>(offsetof(V, r)));
    glBindVertexArray(0);
    return shader_.load("ui");
}

void Ui::shutdown() {
    if (font_) glDeleteTextures(1, &font_);
    if (vbo_) glDeleteBuffers(1, &vbo_);
    if (vao_) glDeleteVertexArrays(1, &vao_);
    font_ = vbo_ = vao_ = 0;
}

void Ui::begin(int w, int h) {
    w_ = w;
    h_ = h;
    verts_.clear();
}

void Ui::quadSolid(float x0, float y0, float x1, float y1, const vec4& c0, const vec4& c1) {
    V a{x0, y0, -1, -1, c0.r, c0.g, c0.b, c0.a}, b{x1, y0, -1, -1, c0.r, c0.g, c0.b, c0.a};
    V c{x1, y1, -1, -1, c1.r, c1.g, c1.b, c1.a}, d{x0, y1, -1, -1, c1.r, c1.g, c1.b, c1.a};
    verts_.insert(verts_.end(), {a, b, c, a, c, d});
}

void Ui::rect(float x, float y, float w, float h, const vec4& c) { quadSolid(x, y, x + w, y + h, c, c); }
void Ui::rectGradV(float x, float y, float w, float h, const vec4& top, const vec4& bottom) {
    quadSolid(x, y, x + w, y + h, top, bottom);
}
void Ui::rectOutline(float x, float y, float w, float h, float t, const vec4& c) {
    rect(x, y, w, t, c);
    rect(x, y + h - t, w, t, c);
    rect(x, y + t, t, h - 2 * t, c);
    rect(x + w - t, y + t, t, h - 2 * t, c);
}

void Ui::line(float x0, float y0, float x1, float y1, float t, const vec4& c) {
    vec2 d(x1 - x0, y1 - y0);
    float l = glm::length(d);
    if (l < 1e-4f) return;
    vec2 n(-d.y / l * t * 0.5f, d.x / l * t * 0.5f);
    V a{x0 + n.x, y0 + n.y, -1, -1, c.r, c.g, c.b, c.a}, b{x1 + n.x, y1 + n.y, -1, -1, c.r, c.g, c.b, c.a};
    V cc{x1 - n.x, y1 - n.y, -1, -1, c.r, c.g, c.b, c.a}, dd{x0 - n.x, y0 - n.y, -1, -1, c.r, c.g, c.b, c.a};
    verts_.insert(verts_.end(), {a, b, cc, a, cc, dd});
}

void Ui::triangle(vec2 a, vec2 b, vec2 c, const vec4& col) {
    V va{a.x, a.y, -1, -1, col.r, col.g, col.b, col.a}, vb{b.x, b.y, -1, -1, col.r, col.g, col.b, col.a};
    V vc{c.x, c.y, -1, -1, col.r, col.g, col.b, col.a};
    verts_.insert(verts_.end(), {va, vb, vc});
}

void Ui::circle(float cx, float cy, float r, const vec4& c, int segs) {
    for (int i = 0; i < segs; ++i) {
        float a0 = kTwoPi * i / segs, a1 = kTwoPi * (i + 1) / segs;
        triangle(vec2(cx, cy), vec2(cx + std::cos(a0) * r, cy + std::sin(a0) * r), vec2(cx + std::cos(a1) * r, cy + std::sin(a1) * r), c);
    }
}

void Ui::ring(float cx, float cy, float r0, float r1, float a0, float a1, const vec4& c, int segs) {
    int n = std::max(1, static_cast<int>(segs * std::fabs(a1 - a0) / kTwoPi) + 1);
    for (int i = 0; i < n; ++i) {
        float s0 = a0 + (a1 - a0) * i / n, s1 = a0 + (a1 - a0) * (i + 1) / n;
        vec2 d0(std::sin(s0), -std::cos(s0)), d1(std::sin(s1), -std::cos(s1));
        vec2 p00 = vec2(cx, cy) + d0 * r0, p01 = vec2(cx, cy) + d0 * r1;
        vec2 p10 = vec2(cx, cy) + d1 * r0, p11 = vec2(cx, cy) + d1 * r1;
        triangle(p00, p01, p11, c);
        triangle(p00, p11, p10, c);
    }
}

void Ui::polyline(const std::vector<vec2>& pts, float t, const vec4& c) {
    for (size_t i = 0; i + 1 < pts.size(); ++i) line(pts[i].x, pts[i].y, pts[i + 1].x, pts[i + 1].y, t, c);
}

void Ui::brackets(float x, float y, float w, float h, float len, float t, const vec4& c) {
    rect(x, y, len, t, c); rect(x, y, t, len, c);
    rect(x + w - len, y, len, t, c); rect(x + w - t, y, t, len, c);
    rect(x, y + h - t, len, t, c); rect(x, y + h - len, t, len, c);
    rect(x + w - len, y + h - t, len, t, c); rect(x + w - t, y + h - len, t, len, c);
}

float Ui::textWidth(const std::string& s, float scale) const {
    float w = 0.f, best = 0.f;
    for (char ch : s) {
        if (ch == '\n') { best = std::max(best, w); w = 0.f; continue; }
        w += 6.0f * scale;
    }
    return std::max(best, w);
}

void Ui::text(float x, float y, const std::string& s, float scale, const vec4& c, Align a) {
    float cx = x, cy = y;
    if (a != Align::Left) {
        // align first line only; multi-line text is aligned per line
    }
    auto lineStart = [&](size_t from) {
        size_t e = s.find('\n', from);
        std::string ln = s.substr(from, e == std::string::npos ? std::string::npos : e - from);
        float w = textWidth(ln, scale);
        return a == Align::Center ? x - w * 0.5f : (a == Align::Right ? x - w : x);
    };
    cx = lineStart(0);
    size_t idx = 0;
    const float aw = static_cast<float>(kCols * kCell), ah = static_cast<float>(kRows * kCell);
    for (char ch : s) {
        ++idx;
        if (ch == '\n') {
            cy += 9.0f * scale;
            cx = lineStart(idx);
            continue;
        }
        int g = static_cast<unsigned char>(ch) - 32;
        if (g < 0 || g >= 95) g = '?' - 32;
        if (g != 0) {
            float u0 = ((g % kCols) * kCell + 1) / aw, v0 = ((g / kCols) * kCell) / ah;
            float u1 = u0 + 5.0f / aw, v1 = v0 + 7.0f / ah;
            float x0 = std::floor(cx), y0 = std::floor(cy), x1 = x0 + 5.0f * scale, y1 = y0 + 7.0f * scale;
            V va{x0, y0, u0, v0, c.r, c.g, c.b, c.a}, vb{x1, y0, u1, v0, c.r, c.g, c.b, c.a};
            V vc{x1, y1, u1, v1, c.r, c.g, c.b, c.a}, vd{x0, y1, u0, v1, c.r, c.g, c.b, c.a};
            verts_.insert(verts_.end(), {va, vb, vc, va, vc, vd});
        }
        cx += 6.0f * scale;
    }
}

void Ui::flush() {
    if (verts_.empty() || !shader_.valid()) {
        verts_.clear();
        return;
    }
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    shader_.use();
    shader_.set("u_screen", vec2(static_cast<float>(w_), static_cast<float>(h_)));
    shader_.set("u_font", 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, font_);
    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(verts_.size() * sizeof(V)), verts_.data(), GL_STREAM_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(verts_.size()));
    glBindVertexArray(0);
    glDisable(GL_BLEND);
    verts_.clear();
}

}  // namespace aegis
