#pragma once
// Frame renderer: shadow pass, HDR scene (visible or thermal), zoomed optics pass, bloom,
// composite, device image + procedural device frames, HUD.

#include <memory>
#include <string>

#include "game/hud.h"
#include "game/mission.h"
#include "gfx/assets.h"
#include "gfx/cab.h"
#include "gfx/gl_util.h"
#include "gfx/mesh.h"
#include "gfx/terrain.h"
#include "gfx/textures.h"
#include "gfx/ui.h"

namespace aegis {

struct Camera {
    vec3 pos{0.f}, fwd{0, 0, -1}, right{1, 0, 0}, up{0, 1, 0};
    mat4 view{1.f}, proj{1.f}, vp{1.f};
    float fovY = 1.f, aspect = 1.f, nearZ = 0.1f, farZ = 9000.f;
    int w = 1, h = 1;
};

class Renderer {
public:
    bool init(int w, int h, const SceneDef& scene, const Environment& env, Terrain& terrain, AssetCatalog& assets, int msaa);
    void resize(int w, int h);
    void render(const Mission& m, float dt);
    void shutdown();
    int width() const { return W_; }
    int height() const { return H_; }

private:
    struct Light { vec3 pos, col; float range; };

    DeviceLayout layout(const Mission& m) const;
    Camera makeCamera(const vec3& pos, const vec3& dir, float fovY, int w, int h, float nearZ) const;
    void gatherLights(const Mission& m);
    void setCommon(const Shader& s, const Camera& c, bool thermal, const Mission& m);
    void shadowPass(const Mission& m);
    void drawScene(const Camera& c, bool thermal, const Mission& m, bool viewmodel);
    void drawSky(const Camera& c, bool thermal, const Mission& m);
    void drawModel(const Shader& sh, const Model& model, const mat4& root, bool thermal, const Mission& m, float time, bool shadowOnly,
                   float rotorPhase = 0.f, bool isCab = false);
    void bindMaterial(const Shader& sh, const Material& mt, float heat);
    void drawProps(const Camera& c, bool thermal, const Mission& m, bool shadow);
    void drawDrones(const Camera& c, bool thermal, const Mission& m, bool shadow);
    void drawLines(const Camera& c, bool thermal, const Mission& m);
    void drawParticles(const Camera& c, bool thermal, const Mission& m);
    void drawRain(const Camera& c, bool thermal, const Mission& m);
    void drawViewmodel(const Camera& c, bool thermal, const Mission& m);
    void fullscreen() const;
    void resolve(const RenderTarget& msaa, const RenderTarget& dst) const;
    void bloomPass();
    void composite(const Mission& m);
    void drawDeviceImage(const Mission& m, const DeviceLayout& d);
    void drawDeviceFrame(const Mission& m, const DeviceLayout& d);
    bool ensureZoomTarget(int w, int h);

    int W_ = 0, H_ = 0, msaa_ = 4;
    const SceneDef* scene_ = nullptr;
    const Environment* env_ = nullptr;
    Terrain* terrain_ = nullptr;
    AssetCatalog* assets_ = nullptr;

    Shader sky_, terrainSh_, water_, obj_, objInst_, shadowSh_, shadowInst_, particle_, rain_, line_, bloomDown_, bloomBlur_, composite_,
        device_, frame_;
    Ui ui_;
    Hud hud_;
    Mesh terrainMesh_, waterMesh_;
    std::unique_ptr<Model> cab_;
    GLuint emptyVao_ = 0, instVbo_ = 0, partVao_ = 0, partVbo_ = 0, lineVao_ = 0, lineVbo_ = 0, wireVao_ = 0, wireVbo_ = 0;
    int wireCount_ = 0;
    GLuint defaultOrm_ = 0;

    RenderTarget scene_msaa_, scene_res_, zoom_msaa_, zoom_res_, bloomA_, bloomB_, bloomC_;
    ShadowTarget shadow_;
    mat4 shadowVP_{1.f};
    bool shadowOn_ = false;
    Light lights_[4];
    int numLights_ = 0;
    float lampLevel_ = 0.f;
    float time_ = 0.f;
    DeviceLayout dev_;
    Camera zoomCam_;
    std::vector<mat4> instScratch_;
    std::vector<float> lineScratch_;
};

}  // namespace aegis
