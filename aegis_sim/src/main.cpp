// FitWingSim - first-person observation-post drone threat simulator.
//
// Command line (all optional; the trainer passes --scene and --out):
//   --scene <file>     scene_definition.json   (default: ./scene_definition.json next to the exe / cwd)
//   --out <file>       player_performance.json (default: next to the scene file)
//   --windowed         run in a window instead of borderless fullscreen   --width/--height <px>
//   --seed <n>         override the random seed (default derived from mission id)
//   --msaa <n>         multisampling samples (default 4)
// Test / tooling options:
//   --shot <file.tga>  render and save a screenshot, then exit  (--frames N, --advance sec, --yaw deg, --pitch deg,
//                      --item 0..5, --raise, --zoom z, --bot)

#include <chrono>
#include <cstdlib>
#include <cstring>
#include <string>

#include "core/common.h"
#include <GLFW/glfw3.h>
#include "core/paths.h"
#include "core/scene.h"
#include "game/mission.h"
#include "gfx/assets.h"
#include "gfx/environment.h"
#include "gfx/renderer.h"
#include "gfx/terrain.h"
#include "gfx/textures.h"

using namespace aegis;

namespace {
struct Args {
    std::string scene, out, shot;
    bool windowed = false, bot = false, raise = false, norender = false;
    int width = 1600, height = 900, msaa = 4, frames = 45, item = -1;
    long seed = -1;
    float advance = 0.f, yaw = -999.f, pitch = -999.f, zoom = 0.f;
};

Args parseArgs(int argc, char** argv) {
    Args a;
    for (int i = 1; i < argc; ++i) {
        std::string k = argv[i];
        auto next = [&]() -> std::string { return i + 1 < argc ? argv[++i] : ""; };
        if (k == "--scene") a.scene = next();
        else if (k == "--out") a.out = next();
        else if (k == "--windowed") a.windowed = true;
        else if (k == "--width") a.width = std::atoi(next().c_str());
        else if (k == "--height") a.height = std::atoi(next().c_str());
        else if (k == "--seed") a.seed = std::atol(next().c_str());
        else if (k == "--msaa") a.msaa = std::atoi(next().c_str());
        else if (k == "--shot") a.shot = next();
        else if (k == "--frames") a.frames = std::atoi(next().c_str());
        else if (k == "--advance") a.advance = static_cast<float>(std::atof(next().c_str()));
        else if (k == "--yaw") a.yaw = static_cast<float>(std::atof(next().c_str()));
        else if (k == "--pitch") a.pitch = static_cast<float>(std::atof(next().c_str()));
        else if (k == "--item") a.item = std::atoi(next().c_str());
        else if (k == "--zoom") a.zoom = static_cast<float>(std::atof(next().c_str()));
        else if (k == "--raise") a.raise = true;
        else if (k == "--bot") a.bot = true;
        else if (k == "--norender") a.norender = true;
    }
    return a;
}

// --- input state fed by GLFW callbacks -------------------------------------------------------
struct InputState {
    double lastX = 0, lastY = 0;
    bool haveLast = false;
    float dx = 0, dy = 0, scroll = 0;
    bool lmb = false, rmb = false, space = false, reload = false, prev = false, k1 = false, k2 = false, k3 = false, enter = false, esc = false;
    bool shot = false;
} g_in;

void cursorCb(GLFWwindow*, double x, double y) {
    if (g_in.haveLast) {
        g_in.dx += static_cast<float>(x - g_in.lastX);
        g_in.dy += static_cast<float>(y - g_in.lastY);
    }
    g_in.lastX = x;
    g_in.lastY = y;
    g_in.haveLast = true;
}
void mouseCb(GLFWwindow*, int b, int action, int) {
    if (action != GLFW_PRESS) return;
    if (b == GLFW_MOUSE_BUTTON_LEFT) g_in.lmb = true;
    if (b == GLFW_MOUSE_BUTTON_RIGHT) g_in.rmb = true;
}
void scrollCb(GLFWwindow*, double, double y) { g_in.scroll += static_cast<float>(y); }
void keyCb(GLFWwindow*, int key, int, int action, int) {
    if (action != GLFW_PRESS) return;
    switch (key) {
        case GLFW_KEY_SPACE: g_in.space = true; break;
        case GLFW_KEY_R: g_in.reload = true; break;
        case GLFW_KEY_Q: g_in.prev = true; break;
        case GLFW_KEY_1: g_in.k1 = true; break;
        case GLFW_KEY_2: g_in.k2 = true; break;
        case GLFW_KEY_3: g_in.k3 = true; break;
        case GLFW_KEY_ENTER:
        case GLFW_KEY_KP_ENTER: g_in.enter = true; break;
        case GLFW_KEY_ESCAPE: g_in.esc = true; break;
        case GLFW_KEY_F12: g_in.shot = true; break;
        default: break;
    }
}

Input takeInput(GLFWwindow* win) {
    Input in;
    in.mdx = g_in.dx;
    in.mdy = g_in.dy;
    in.scroll = g_in.scroll;
    in.lmb = g_in.lmb;
    in.rmb = g_in.rmb;
    in.space = g_in.space;
    in.reload = g_in.reload;
    in.prevItem = g_in.prev;
    in.key1 = g_in.k1;
    in.key2 = g_in.k2;
    in.key3 = g_in.k3;
    in.enter = g_in.enter;
    in.escape = g_in.esc;
    in.steady = glfwGetKey(win, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS || glfwGetKey(win, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS;
    g_in.dx = g_in.dy = g_in.scroll = 0.f;
    g_in.lmb = g_in.rmb = g_in.space = g_in.reload = g_in.prev = g_in.k1 = g_in.k2 = g_in.k3 = g_in.enter = g_in.esc = false;
    return in;
}

// A very small autopilot used for headless verification of the mission logic.
struct Bot {
    float t = 0.f;
    int step = 0;
    Input act(const Mission& m, float dt) {
        Input in;
        const World& w = m.w;
        t += dt;
        in.steady = true;
        if (w.phase == Phase::Briefing) {
            in.enter = true;
            return in;
        }
        if (w.phase != Phase::Running) {
            if (w.endTimer > 2.5f) in.enter = true;
            return in;
        }
        Player p = w.player;
        // choose aim target: the nearest alive drone, otherwise the first swarm bearing
        vec3 target(0, 0, 0);
        bool have = false;
        float bd = 1e9f;
        for (const auto& d : w.drones)
            if (d.alive() && glm::length(d.pos) < bd) { bd = glm::length(d.pos); target = d.pos; have = true; }
        float wantYaw = p.yaw, wantPitch = p.pitch;
        if (have) {
            vec3 to = target - p.eye();
            wantYaw = std::atan2(to.x, -to.z);
            wantPitch = std::atan2(to.y, std::sqrt(to.x * to.x + to.z * to.z));
        } else if (!w.swarms.empty()) {
            wantYaw = w.swarms[0].bearing;
            wantPitch = 0.15f;
        }
        float dy = wrapAngle(wantYaw - w.player.yaw);
        float dp = wantPitch - w.player.pitch;
        float zoomEff = 1.f + (w.player.zoom[static_cast<int>(w.player.item)] - 1.f) * w.player.raise;
        float sens = 0.0021f / std::pow(zoomEff, 0.85f);
        in.mdx = glm::clamp(dy / sens, -90.f, 90.f);
        in.mdy = glm::clamp(-dp / sens, -90.f, 90.f);
        Item want = Item::Thermal;
        if (w.stage == Stage::Detect) want = m.itemUsable(Item::Binoculars) ? Item::Binoculars : Item::Acoustic;
        if (w.stage == Stage::Respond) want = (m.scene().threatType == ThreatType::Single) ? Item::Rifle : Item::Launcher;
        if (w.stage == Stage::Detect && w.threatTime > 6.f) want = Item::Acoustic;
        if (w.stage == Stage::Classify) {
            in.key1 = m.scene().threatType == ThreatType::Single;
            in.key2 = m.scene().threatType == ThreatType::Swarm;
            in.key3 = m.scene().threatType == ThreatType::MultipleSwarm;
        }
        if (p.item != want && p.itemSwitchT < 0.5f) {
            if (static_cast<int>(t * 10) % 4 == 0) in.rmb = true;
        } else if (!p.inUse && p.raise < 0.05f && p.itemSwitchT < 0.5f) {
            in.lmb = true;
        } else if (p.raise > 0.95f) {
            if (static_cast<int>(t * 4) % 2 == 0 && (p.item != Item::Launcher || p.locked)) in.space = true;
        }
        return in;
    }
};
}  // namespace

int main(int argc, char** argv) {
    Args args = parseArgs(argc, argv);

    std::string sceneFile = args.scene;
    if (sceneFile.empty()) {
        for (const std::string& c : {std::string("scene_definition.json"), exeDir() + "/scene_definition.json"})
            if (fileExists(c)) { sceneFile = c; break; }
    }
    if (sceneFile.empty()) {
        LOG_ERROR("no scene_definition.json found (use --scene <file>)");
        return 2;
    }
    SceneDef scene;
    std::string err;
    if (!loadScene(sceneFile, scene, err)) {
        LOG_ERROR("scene error: %s", err.c_str());
        return 2;
    }
    std::string outFile = args.out;
    if (outFile.empty()) {
        size_t sl = sceneFile.find_last_of("/\\");
        outFile = (sl == std::string::npos ? std::string() : sceneFile.substr(0, sl + 1)) + "player_performance.json";
    }

    if (!glfwInit()) {
        LOG_ERROR("glfwInit failed");
        return 3;
    }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    GLFWmonitor* mon = nullptr;
    int ww = args.width, wh = args.height;
    bool headless = !args.shot.empty();
    if (!args.windowed && !headless) {
        mon = glfwGetPrimaryMonitor();
        const GLFWvidmode* vm = glfwGetVideoMode(mon);
        glfwWindowHint(GLFW_RED_BITS, vm->redBits);
        glfwWindowHint(GLFW_GREEN_BITS, vm->greenBits);
        glfwWindowHint(GLFW_BLUE_BITS, vm->blueBits);
        glfwWindowHint(GLFW_REFRESH_RATE, vm->refreshRate);
        ww = vm->width;
        wh = vm->height;
    }
    GLFWwindow* win = glfwCreateWindow(ww, wh, "FITWING - Observation Post", mon, nullptr);
    if (!win) {
        LOG_ERROR("could not create an OpenGL 3.3 window");
        glfwTerminate();
        return 3;
    }
    glfwMakeContextCurrent(win);
    glfwSwapInterval(headless ? 0 : 1);
    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) {
        LOG_ERROR("glewInit failed");
        return 3;
    }
    glGetError();
    LOG_INFO("GL %s | %s", reinterpret_cast<const char*>(glGetString(GL_VERSION)), reinterpret_cast<const char*>(glGetString(GL_RENDERER)));
    glfwSetCursorPosCallback(win, cursorCb);
    glfwSetMouseButtonCallback(win, mouseCb);
    glfwSetScrollCallback(win, scrollCb);
    glfwSetKeyCallback(win, keyCb);
    if (!headless) {
        glfwSetInputMode(win, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        if (glfwRawMouseMotionSupported()) glfwSetInputMode(win, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
    }

    std::string shaderDir = findRuntimeDir("shaders");
    std::string resDir = findRuntimeDir("resources");
    if (shaderDir.empty()) {
        LOG_ERROR("shaders/ folder not found next to the executable");
        return 4;
    }
    Shader::setShaderDir(shaderDir);
    LOG_INFO("shaders: %s | resources: %s", shaderDir.c_str(), resDir.empty() ? "(none)" : resDir.c_str());

    uint32_t seed = args.seed >= 0 ? static_cast<uint32_t>(args.seed) : static_cast<uint32_t>(scene.missionId * 7919 + 13);
    int fbw, fbh;
    glfwGetFramebufferSize(win, &fbw, &fbh);

    TextureLibrary::get().init(resDir);
    if (const char* dumpDir = std::getenv("FITWING_DUMP_TEXTURES")) {  // developer tool: write the procedural textures as PPM
        for (int k = 0; k < static_cast<int>(TexKind::Count); ++k) {
            const TexSet& ts = TextureLibrary::get().set(static_cast<TexKind>(k));
            const GLuint ids[3] = {ts.albedo, ts.normal, ts.orm};
            const char* nm[3] = {"albedo", "normal", "orm"};
            for (int i = 0; i < 3; ++i) {
                GLint tw = 0, th = 0;
                glBindTexture(GL_TEXTURE_2D, ids[i]);
                glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &tw);
                glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &th);
                std::vector<unsigned char> px(static_cast<size_t>(tw) * th * 3);
                glPixelStorei(GL_PACK_ALIGNMENT, 1);
                glGetTexImage(GL_TEXTURE_2D, 0, GL_RGB, GL_UNSIGNED_BYTE, px.data());
                std::string fn = std::string(dumpDir) + "/" + texKindName(static_cast<TexKind>(k)) + "_" + nm[i] + ".ppm";
                FILE* f = std::fopen(fn.c_str(), "wb");
                if (!f) continue;
                std::fprintf(f, "P6\n%d %d\n255\n", tw, th);
                std::fwrite(px.data(), 1, px.size(), f);
                std::fclose(f);
            }
        }
    }
    AssetCatalog assets;
    assets.load(resDir);
    Environment env = makeEnvironment(scene, seed);
    Terrain terrain;
    terrain.init(scene.terrain, seed);
    terrain.placeProps();
    Renderer renderer;
    if (!renderer.init(fbw, fbh, scene, env, terrain, assets, args.msaa)) return 5;
    Mission mission(scene, env, terrain, seed);

    // test overrides ---------------------------------------------------------------------------------
    if (args.yaw > -900.f) mission.w.player.yaw = radians(args.yaw);
    if (args.pitch > -900.f) mission.w.player.pitch = radians(args.pitch);
    Bot bot;
    auto last = std::chrono::steady_clock::now();
    int frame = 0;
    bool started = false;
    bool running = true;
    float fixedDt = 1.f / 30.f;
    while (running) {
        glfwPollEvents();
        auto now = std::chrono::steady_clock::now();
        float dt = std::chrono::duration<float>(now - last).count();
        last = now;
        if (headless) dt = fixedDt;
        Input in = takeInput(win);
        if (glfwWindowShouldClose(win)) in.escape = true;
        if (args.bot) in = bot.act(mission, dt);
        if (headless && !started) {
            in.enter = true;  // skip the briefing
            started = true;
            // fast-forward
            float t = 0.f;
            Input startIn;
            startIn.enter = true;
            mission.update(fixedDt, startIn);
            while (t < args.advance) {
                Input bi = args.bot ? bot.act(mission, fixedDt) : Input();
                mission.update(fixedDt, bi);
                t += fixedDt;
            }
            if (args.item >= 0) mission.w.player.item = static_cast<Item>(args.item);
            if (args.raise) {
                mission.w.player.inUse = true;
                mission.w.player.raise = 1.f;
            }
            if (args.zoom > 0.f) mission.w.player.zoom[static_cast<int>(mission.w.player.item)] = args.zoom;
            in = Input();
        }
        mission.update(dt, in);
        int cw, ch;
        glfwGetFramebufferSize(win, &cw, &ch);
        if (cw > 0 && ch > 0 && (cw != renderer.width() || ch != renderer.height())) renderer.resize(cw, ch);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        if (!args.norender) renderer.render(mission, dt);
        if (g_in.shot || (headless && frame == args.frames)) {
            g_in.shot = false;
            glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
            std::string name = headless ? args.shot : "fitwing_shot_" + std::to_string(frame) + ".tga";
            saveScreenshotTGA(name, renderer.width(), renderer.height());
            LOG_INFO("screenshot: %s", name.c_str());
            if (headless) running = false;
        }
        checkGlError("frame");
        glfwSwapBuffers(win);
        ++frame;
        if (mission.finished()) running = false;
        if (headless && args.bot && frame > 60 * 30 * 6) running = false;
    }
    // report
    if (!headless || args.bot) {
        mission.update(0.f, Input());
        if (mission.w.phase == Phase::Running) {
            Input esc;
            esc.escape = true;
            mission.update(0.f, esc);
        }
        if (mission.writePerformance(outFile)) LOG_INFO("wrote %s", outFile.c_str());
        else LOG_ERROR("could not write %s", outFile.c_str());
    }
    renderer.shutdown();
    glfwDestroyWindow(win);
    glfwTerminate();
    return 0;
}
