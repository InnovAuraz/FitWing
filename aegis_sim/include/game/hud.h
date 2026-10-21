#pragma once
// 2D overlay: status strip, compass, messages, device reticles, sensor tablets, briefing and outcome screens.

#include "game/mission.h"
#include "gfx/ui.h"

namespace aegis {

// Where the active device is drawn (pixels, origin top-left).
struct DeviceLayout {
    bool active = false;       // a device is (being) raised
    Item item = Item::Binoculars;
    int style = 0;             // frame style: 0 binoculars, 1 thermal, 2 tablet, 3 scope
    vec4 outer{0.f};           // x0,y0,x1,y1
    vec4 hole{0.f};
    float cornerOuter = 0.f, cornerHole = 0.f;
    bool circle = false;       // scope: circular image
    bool hasImage = false;     // zoomed camera image inside the hole
    float blackout = 0.f;
    mat4 viewProj{1.f};        // view-projection of the zoomed image (for markers)
    vec3 camPos{0.f};
    float zoom = 1.f;
    float fovY = 1.f;
    float offsetY = 0.f;       // slide-in animation offset in pixels (already applied to rects)
};

class Hud {
public:
    void draw(Ui& ui, const Mission& m, const DeviceLayout& dev, float time);

private:
    void status(Ui& ui, const Mission& m, float time);
    void compass(Ui& ui, const Mission& m);
    void messages(Ui& ui, const Mission& m);
    void hotbar(Ui& ui, const Mission& m);
    void reticle(Ui& ui, const Mission& m, const DeviceLayout& dev, float time);
    void acousticTablet(Ui& ui, const Mission& m, const DeviceLayout& dev, float time);
    void rfTablet(Ui& ui, const Mission& m, const DeviceLayout& dev, float time);
    void briefing(Ui& ui, const Mission& m, float time);
    void outcome(Ui& ui, const Mission& m, float time);
    void jumpscare(Ui& ui, const Mission& m, float time);
    void overlays(Ui& ui, const Mission& m, float time);
};

}  // namespace aegis
