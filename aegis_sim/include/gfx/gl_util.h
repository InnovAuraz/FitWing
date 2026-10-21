#pragma once
// Thin OpenGL helpers: shader programs (with #include and #define injection),
// textures and framebuffer objects.

#include <string>
#include <unordered_map>
#include <vector>

#include "core/common.h"

namespace aegis {

class Shader {
public:
    Shader() = default;
    ~Shader();
    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;

    // Loads <shaders>/<name>.vert and <name>.frag. `defines` become "#define X" lines right after #version.
    // Reloading (calling load again) keeps the old program if the new one fails to compile.
    bool load(const std::string& name, const std::vector<std::string>& defines = {});
    bool reload();
    void use() const { glUseProgram(prog_); }
    bool valid() const { return prog_ != 0; }
    GLuint id() const { return prog_; }

    GLint loc(const char* name) const;
    void set(const char* n, int v) const { glUniform1i(loc(n), v); }
    void set(const char* n, float v) const { glUniform1f(loc(n), v); }
    void set(const char* n, const vec2& v) const { glUniform2fv(loc(n), 1, &v.x); }
    void set(const char* n, const vec3& v) const { glUniform3fv(loc(n), 1, &v.x); }
    void set(const char* n, const vec4& v) const { glUniform4fv(loc(n), 1, &v.x); }
    void set(const char* n, const mat3& m) const { glUniformMatrix3fv(loc(n), 1, GL_FALSE, glm::value_ptr(m)); }
    void set(const char* n, const mat4& m) const { glUniformMatrix4fv(loc(n), 1, GL_FALSE, glm::value_ptr(m)); }

    // Sets the shader directory used by all Shader instances.
    static void setShaderDir(const std::string& dir) { shaderDir_ = dir; }
    static const std::string& shaderDir() { return shaderDir_; }

private:
    static std::string loadSource(const std::string& file, int depth, bool& ok);
    static GLuint compile(GLenum type, const std::string& src, const std::string& label);

    GLuint prog_ = 0;
    std::string name_;
    std::vector<std::string> defines_;
    mutable std::unordered_map<std::string, GLint> locCache_;
    static std::string shaderDir_;
};

// 2D texture helper ---------------------------------------------------------------------------
GLuint createTexture2D(int w, int h, GLenum internalFormat, GLenum format, GLenum type, const void* data, bool mipmaps,
                       GLenum minFilter = GL_LINEAR_MIPMAP_LINEAR, GLenum magFilter = GL_LINEAR, GLenum wrap = GL_REPEAT);
void setAnisotropy(GLuint tex, float amount);

// Offscreen render target (optionally multisampled).
struct RenderTarget {
    GLuint fbo = 0;
    GLuint colorTex = 0;  // texture (single-sample) or 0 when using renderbuffers
    GLuint colorRb = 0;   // multisampled colour renderbuffer
    GLuint depthRb = 0;
    GLuint depthTex = 0;
    int w = 0, h = 0;
    int samples = 0;
    GLenum format = GL_RGBA16F;

    // Colour texture + depth renderbuffer (single sample) or MSAA renderbuffers.
    bool create(int width, int height, GLenum colorFormat, int msaaSamples, bool depthAsTexture = false);
    void destroy();
    void bind() const;
};

// Depth only target for shadow mapping.
struct ShadowTarget {
    GLuint fbo = 0;
    GLuint depthTex = 0;
    int size = 0;
    bool create(int sz);
    void destroy();
};

bool checkGlError(const char* where);

// Writes the currently bound read framebuffer to an uncompressed 24-bit TGA file.
bool saveScreenshotTGA(const std::string& path, int w, int h);

}  // namespace aegis
