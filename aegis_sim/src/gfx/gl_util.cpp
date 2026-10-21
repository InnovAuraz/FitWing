#include "gfx/gl_util.h"

#include <cstring>
#include <fstream>
#include <sstream>

#include "core/paths.h"

namespace aegis {

std::string Shader::shaderDir_;

Shader::~Shader() {
    if (prog_) glDeleteProgram(prog_);
}

std::string Shader::loadSource(const std::string& file, int depth, bool& ok) {
    if (depth > 8) {
        LOG_ERROR("shader include nesting too deep: %s", file.c_str());
        ok = false;
        return "";
    }
    std::string text;
    if (!readTextFile(shaderDir_ + "/" + file, text)) {
        LOG_ERROR("cannot read shader file %s/%s", shaderDir_.c_str(), file.c_str());
        ok = false;
        return "";
    }
    std::istringstream in(text);
    std::string line, out;
    while (std::getline(in, line)) {
        // Detect:  #include "name.glsl"
        size_t p = line.find("#include");
        if (p != std::string::npos && line.find_first_not_of(" \t") == p) {
            size_t a = line.find('"', p);
            size_t b = (a == std::string::npos) ? a : line.find('"', a + 1);
            if (a != std::string::npos && b != std::string::npos) {
                std::string inc = line.substr(a + 1, b - a - 1);
                out += loadSource(inc, depth + 1, ok);
                out += "\n";
                continue;
            }
        }
        out += line;
        out += "\n";
    }
    return out;
}

static std::string injectDefines(const std::string& src, const std::vector<std::string>& defines) {
    if (defines.empty()) return src;
    size_t p = src.find("#version");
    size_t eol = (p == std::string::npos) ? 0 : src.find('\n', p);
    if (eol == std::string::npos) eol = src.size();
    else ++eol;
    std::string d;
    for (const auto& s : defines) d += "#define " + s + "\n";
    return src.substr(0, eol) + d + src.substr(eol);
}

GLuint Shader::compile(GLenum type, const std::string& src, const std::string& label) {
    GLuint s = glCreateShader(type);
    const char* c = src.c_str();
    glShaderSource(s, 1, &c, nullptr);
    glCompileShader(s);
    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[4096];
        glGetShaderInfoLog(s, sizeof(log), nullptr, log);
        LOG_ERROR("shader compile failed (%s):\n%s", label.c_str(), log);
        glDeleteShader(s);
        return 0;
    }
    return s;
}

bool Shader::load(const std::string& name, const std::vector<std::string>& defines) {
    bool ok = true;
    // Full-screen post-processing programs share fullscreen.vert.
    std::string vname = name + ".vert";
    if (!fileExists(shaderDir_ + "/" + vname)) vname = "fullscreen.vert";
    std::string vs = loadSource(vname, 0, ok);
    std::string fs = loadSource(name + ".frag", 0, ok);
    if (!ok) return false;
    vs = injectDefines(vs, defines);
    fs = injectDefines(fs, defines);
    GLuint v = compile(GL_VERTEX_SHADER, vs, name + ".vert");
    GLuint f = compile(GL_FRAGMENT_SHADER, fs, name + ".frag");
    if (!v || !f) {
        if (v) glDeleteShader(v);
        if (f) glDeleteShader(f);
        return false;
    }
    GLuint p = glCreateProgram();
    glAttachShader(p, v);
    glAttachShader(p, f);
    glLinkProgram(p);
    glDeleteShader(v);
    glDeleteShader(f);
    GLint linked = 0;
    glGetProgramiv(p, GL_LINK_STATUS, &linked);
    if (!linked) {
        char log[4096];
        glGetProgramInfoLog(p, sizeof(log), nullptr, log);
        LOG_ERROR("shader link failed (%s): %s", name.c_str(), log);
        glDeleteProgram(p);
        return false;
    }
    if (prog_) glDeleteProgram(prog_);
    prog_ = p;
    name_ = name;
    defines_ = defines;
    locCache_.clear();
    return true;
}

bool Shader::reload() {
    if (name_.empty()) return false;
    std::string n = name_;
    std::vector<std::string> d = defines_;
    return load(n, d);
}

GLint Shader::loc(const char* name) const {
    auto it = locCache_.find(name);
    if (it != locCache_.end()) return it->second;
    GLint l = glGetUniformLocation(prog_, name);
    locCache_[name] = l;
    return l;
}

// ---------------------------------------------------------------------------------------------
GLuint createTexture2D(int w, int h, GLenum internalFormat, GLenum format, GLenum type, const void* data, bool mipmaps,
                       GLenum minFilter, GLenum magFilter, GLenum wrap) {
    GLuint t = 0;
    glGenTextures(1, &t);
    glBindTexture(GL_TEXTURE_2D, t);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, static_cast<GLint>(internalFormat), w, h, 0, format, type, data);
    if (mipmaps) glGenerateMipmap(GL_TEXTURE_2D);
    else if (minFilter == GL_LINEAR_MIPMAP_LINEAR || minFilter == GL_NEAREST_MIPMAP_NEAREST) minFilter = GL_LINEAR;
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, static_cast<GLint>(minFilter));
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, static_cast<GLint>(magFilter));
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, static_cast<GLint>(wrap));
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, static_cast<GLint>(wrap));
    return t;
}

void setAnisotropy(GLuint tex, float amount) {
    if (!GLEW_EXT_texture_filter_anisotropic) return;
    GLfloat maxA = 1.0f;
    glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &maxA);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, std::min(amount, maxA));
}

bool RenderTarget::create(int width, int height, GLenum colorFormat, int msaaSamples, bool depthAsTexture) {
    destroy();
    w = width;
    h = height;
    format = colorFormat;
    samples = msaaSamples;
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    if (msaaSamples > 1) {
        glGenRenderbuffers(1, &colorRb);
        glBindRenderbuffer(GL_RENDERBUFFER, colorRb);
        glRenderbufferStorageMultisample(GL_RENDERBUFFER, msaaSamples, colorFormat, w, h);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, colorRb);
        glGenRenderbuffers(1, &depthRb);
        glBindRenderbuffer(GL_RENDERBUFFER, depthRb);
        glRenderbufferStorageMultisample(GL_RENDERBUFFER, msaaSamples, GL_DEPTH_COMPONENT24, w, h);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depthRb);
    } else {
        GLenum fmt = GL_RGBA;
        GLenum type = GL_FLOAT;
        if (colorFormat == GL_RGBA8) type = GL_UNSIGNED_BYTE;
        if (colorFormat == GL_R16F || colorFormat == GL_R32F) fmt = GL_RED;
        colorTex = createTexture2D(w, h, colorFormat, fmt, type, nullptr, false, GL_LINEAR, GL_LINEAR, GL_CLAMP_TO_EDGE);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorTex, 0);
        if (depthAsTexture) {
            depthTex = createTexture2D(w, h, GL_DEPTH_COMPONENT24, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr, false, GL_NEAREST,
                                       GL_NEAREST, GL_CLAMP_TO_EDGE);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthTex, 0);
        } else {
            glGenRenderbuffers(1, &depthRb);
            glBindRenderbuffer(GL_RENDERBUFFER, depthRb);
            glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, w, h);
            glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depthRb);
        }
    }
    GLenum st = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    if (st != GL_FRAMEBUFFER_COMPLETE) {
        LOG_ERROR("framebuffer incomplete (0x%x) %dx%d samples=%d", st, w, h, msaaSamples);
        return false;
    }
    return true;
}

void RenderTarget::destroy() {
    if (fbo) glDeleteFramebuffers(1, &fbo);
    if (colorTex) glDeleteTextures(1, &colorTex);
    if (colorRb) glDeleteRenderbuffers(1, &colorRb);
    if (depthRb) glDeleteRenderbuffers(1, &depthRb);
    if (depthTex) glDeleteTextures(1, &depthTex);
    fbo = colorTex = colorRb = depthRb = depthTex = 0;
}

void RenderTarget::bind() const {
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glViewport(0, 0, w, h);
}

bool ShadowTarget::create(int sz) {
    destroy();
    size = sz;
    depthTex = createTexture2D(sz, sz, GL_DEPTH_COMPONENT24, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr, false, GL_LINEAR, GL_LINEAR,
                               GL_CLAMP_TO_BORDER);
    float border[4] = {1.f, 1.f, 1.f, 1.f};
    glBindTexture(GL_TEXTURE_2D, depthTex);
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, border);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthTex, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    GLenum st = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    if (st != GL_FRAMEBUFFER_COMPLETE) {
        LOG_ERROR("shadow framebuffer incomplete (0x%x)", st);
        return false;
    }
    return true;
}

void ShadowTarget::destroy() {
    if (fbo) glDeleteFramebuffers(1, &fbo);
    if (depthTex) glDeleteTextures(1, &depthTex);
    fbo = depthTex = 0;
}

bool checkGlError(const char* where) {
    bool any = false;
    GLenum e;
    while ((e = glGetError()) != GL_NO_ERROR) {
        LOG_WARN("GL error 0x%x at %s", e, where);
        any = true;
    }
    return any;
}

bool saveScreenshotTGA(const std::string& path, int w, int h) {
    std::vector<unsigned char> px(static_cast<size_t>(w) * h * 3);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, w, h, GL_BGR, GL_UNSIGNED_BYTE, px.data());
    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    if (!f) return false;
    unsigned char hdr[18] = {0};
    hdr[2] = 2;  // uncompressed true-colour
    hdr[12] = static_cast<unsigned char>(w & 0xFF);
    hdr[13] = static_cast<unsigned char>((w >> 8) & 0xFF);
    hdr[14] = static_cast<unsigned char>(h & 0xFF);
    hdr[15] = static_cast<unsigned char>((h >> 8) & 0xFF);
    hdr[16] = 24;
    f.write(reinterpret_cast<const char*>(hdr), 18);
    f.write(reinterpret_cast<const char*>(px.data()), static_cast<std::streamsize>(px.size()));
    return static_cast<bool>(f);
}

}  // namespace aegis
