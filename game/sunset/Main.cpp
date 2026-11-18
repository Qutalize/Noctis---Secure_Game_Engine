#include "game/sunset/SunsetScene.h"
#include "game/sunset/CoastalAudio.h"

#include "engine/core/ConsoleUtf8.h"
#include "engine/core/Window.h"
#include "engine/math/Collision.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

using namespace DirectX;
namespace fs = std::filesystem;
namespace {
constexpr int kWidth = 960;
constexpr int kHeight = 540;
constexpr int kPvFps = 24;
constexpr int kPvSeconds = 20;
constexpr float kCloseTime = 12.3f;
constexpr float kEyeHeight = 1.62f;
constexpr float kSpeed = 2.5f;
constexpr XMFLOAT3 kHalfBody{.25f, .80f, .25f};

struct Walk {
    XMFLOAT3 center{-.14f, .85f, -7.2f};
    float yaw = 0.f;
    float pitch = .045f;

    void Move(float strafe, float forward, float seconds) {
        const float scale = kSpeed * seconds / std::max(1.f, std::hypot(strafe, forward));
        const XMFLOAT3 delta{scale * (strafe * std::cos(yaw) + forward * std::sin(yaw)), 0.f,
                             scale * (forward * std::cos(yaw) - strafe * std::sin(yaw))};
        // Both asset presets share the same collision footprints.
        static const auto walls = [] {
            std::vector<AABB> boxes;
            for (const float z : {1.f, 5.4f, 9.8f}) {
                boxes.push_back({{-1.47f, 0.f, z - .07f}, {-.70f, 2.26f, z + .81f}});
                boxes.push_back({{1.27f, 0.f, z - .07f}, {2.03f, 2.26f, z + .81f}});
                boxes.push_back({{-3.11f, 0.f, z}, {-2.65f, .75f, z + 3.25f}});
                boxes.push_back({{2.66f, 0.f, z - .05f}, {3.12f, .97f, z + .57f}});
            }
            boxes.push_back({{1.82f, 0.f, 12.94f}, {3.08f, 3.25f, 15.84f}});
            return boxes;
        }();
        const auto next = ResolveAABBCollision(center, kHalfBody, delta, walls);
        const auto walkable = [](float x, float z) {
            const bool pier = x >= -2.55f && x <= 2.55f && z >= -12.75f && z <= 19.2f;
            const bool fishing = x >= -13.38f && x <= -2.55f && z >= -3.42f && z <= -1.33f;
            const bool restaurant = x >= -5.14f && x <= 5.14f && z >= 18.94f && z <= 27.9f;
            const bool villaDeck = x >= 2.55f && x <= 6.7f && z >= -4.20f && z <= 8.35f;
            return pier || fishing || restaurant || villaDeck;
        };
        if (walkable(next.x, center.z)) center.x = next.x;
        if (walkable(center.x, next.z)) center.z = next.z;
    }

    Camera View() const {
        Camera camera;
        camera.SetPosition({center.x, kEyeHeight, center.z});
        camera.SetRotation(yaw, pitch);
        return camera;
    }
};

Camera LookAt(const XMFLOAT3& eye, const XMFLOAT3& target) {
    Camera camera;
    camera.SetPosition(eye);
    const float right = target.x - eye.x;
    const float forward = target.z - eye.z;
    camera.SetRotation(std::atan2(right, forward), std::atan2(target.y - eye.y, std::hypot(right, forward)));
    return camera;
}

float Ease(float progress) {
    const float fraction = std::clamp(progress, 0.f, 1.f);
    return fraction * fraction * (3.f - 2.f * fraction);
}

Camera PvCamera(float time) {
    if (time < 5.f) {
        const float amount = Ease(time / 5.f);
        return LookAt({std::lerp(-14.f, -12.6f, amount), std::lerp(13.2f, 11.8f, amount),
                       std::lerp(-26.f, -23.8f, amount)}, {-2.1f, .5f, 25.f});
    }
    if (time < 9.8f) {
        const float amount = Ease((time - 5.f) / 4.8f);
        return LookAt({std::lerp(-.67f, -.12f, amount), 1.72f, std::lerp(-12.4f, -5.2f, amount)},
                      {-.35f, 1.75f, 22.5f});
    }
    if (time < 14.5f) {
        const float amount = Ease((time - 9.8f) / 4.7f);
        return LookAt({std::lerp(.64f, 2.4f, amount), std::lerp(1.56f, 1.53f, amount),
                       std::lerp(-.92f, .88f, amount)}, {6.25f, 1.48f, 5.64f});
    }
    const float amount = Ease((time - 14.5f) / 5.5f);
    return LookAt({std::lerp(-.38f, .1f, amount), 1.68f, std::lerp(14.36f, 23.7f, amount)},
                  {-.10f, 1.74f, 29.3f});
}

float PvDaylight(float time) {
    return Ease(time / kPvSeconds);
}

void SelfTest() {
    Walk walk;
    const float initialZ = walk.center.z;
    walk.yaw = 0.f;
    for (int i = 0; i < 120; ++i) {
        walk.Move(0.f, 1.f, 1.f / 60.f);
    }
    if (std::abs(walk.center.z - initialZ - 5.f) > .001f) throw std::runtime_error("Forward movement failed");

    Walk column;
    column.center = {-1.10f, .85f, -1.f};
    column.yaw = 0.f;
    for (int i = 0; i < 120; ++i) {
        column.Move(0.f, 1.f, 1.f / 60.f);
    }
    if (std::abs(column.center.z - .68f) > .002f) throw std::runtime_error("Column collision failed");

    for (int i = 0; i < 600; ++i) {
        walk.Move(0.f, -1.f, 1.f / 60.f);
    }
    if (walk.center.z < -12.75f || walk.center.z > -12.70f) throw std::runtime_error("Harbor boundary failed");

    Walk fishing;
    fishing.center = {-2.f, .85f, -2.49f};
    for (int step = 0; step < 350; ++step) fishing.Move(-1.f, 0.f, 1.f / 60.f);
    if (fishing.center.x < -13.38f || fishing.center.x > -13.33f) throw std::runtime_error("Fishing dock route failed");
    fishing.Move(0.f, 1.f, 1.f);
    if (fishing.center.z != -2.49f) throw std::runtime_error("Fishing dock boundary failed");

    Walk dining;
    dining.center = {0.f, .85f, 14.f};
    for (int step = 0; step < 260; ++step) dining.Move(0.f, 1.f, 1.f / 60.f);
    if (dining.center.z < 24.7f) throw std::runtime_error("Restaurant route failed");
    std::puts("Coastal walk checks passed: movement, column, end of pier, fishing dock, restaurant route");
}

void SavePpm(const fs::path& path, const std::vector<uint8_t>& rgba) {
    if (rgba.size() != static_cast<std::size_t>(kWidth * kHeight * 4)) {
        throw std::runtime_error("Unexpected capture size");
    }
    std::vector<uint8_t> rgb;
    rgb.reserve(kWidth * kHeight * 3);
    for (std::size_t i = 0; i < rgba.size(); i += 4) {
        rgb.insert(rgb.end(), {rgba[i], rgba[i + 1], rgba[i + 2]});
    }
    std::ofstream stream(path, std::ios::binary);
    stream << "P6\n" << kWidth << ' ' << kHeight << "\n255\n";
    stream.write(reinterpret_cast<const char*>(rgb.data()), static_cast<std::streamsize>(rgb.size()));
    if (!stream) throw std::runtime_error("Could not write PPM capture");
}

int Capture(const fs::path& directory) {
    if (!fs::create_directory(directory)) throw std::runtime_error("Capture directory must be new");
    Renderer renderer;
    renderer.Initialize(nullptr, kWidth, kHeight);
    if (!renderer.IsPbrAvailable() || !renderer.IsShadowEnabled()) throw std::runtime_error("Missing PBR or shadow pipeline");
    sunset::Scene scene(renderer);

    std::ofstream measurements(directory / "samples.csv");
    measurements << "shot,asset,asset_triangles,sample,warp_cpu_submit_wait_ms\n";
    for (const auto& [shot, time] : {std::pair{"wide", 0.f}, std::pair{"close", kCloseTime}}) {
        for (const auto detail : {sunset::Detail::Simple, sunset::Detail::Detailed}) {
            const std::string asset = detail == sunset::Detail::Simple ? "simple" : "detailed";
            const auto camera = PvCamera(time);
            constexpr int kWarmup = 2;
            constexpr int kSamples = 6;
            for (int i = 0; i < kWarmup + kSamples; ++i) {
                const auto start = std::chrono::steady_clock::now();
                scene.Draw(renderer, camera, detail, time, PvDaylight(time));
                renderer.EndFrame();
                const auto durationMs = std::chrono::duration<double, std::milli>(
                    std::chrono::steady_clock::now() - start).count();
                if (i >= kWarmup) {
                    measurements << shot << ',' << asset << ',' << scene.TriangleCount(detail) << ','
                                 << (i - kWarmup) << ',' << durationMs << '\n';
                }
            }
            SavePpm(directory / (std::string(shot) + "-" + asset + ".ppm"), renderer.ReadOffscreenRgba());
        }
    }
    if (!measurements) throw std::runtime_error("Could not write capture samples");
    const auto seaCamera = LookAt({-8.8f, 4.2f, -3.4f}, {-15.f, -.45f, 15.f});
    for (const auto& [name, time, daylight] : {std::tuple{"sea-first.ppm", 5.f, .24f},
             std::tuple{"sea-later.ppm", 5.62f, .24f}, std::tuple{"sea-dusk.ppm", 5.f, 1.f}}) {
        scene.Draw(renderer, seaCamera, sunset::Detail::Detailed, time, daylight);
        renderer.EndFrame();
        SavePpm(directory / name, renderer.ReadOffscreenRgba());
    }
    renderer.Shutdown();
    return 0;
}

int RenderPv(const fs::path& directory) {
    if (!fs::create_directory(directory)) throw std::runtime_error("PV frame directory must be new");
    Renderer renderer;
    renderer.Initialize(nullptr, kWidth, kHeight);
    if (!renderer.IsPbrAvailable() || !renderer.IsShadowEnabled()) throw std::runtime_error("Missing PBR or shadow pipeline");
    sunset::Scene scene(renderer);
    std::ofstream listener(directory / "listener.csv");
    listener << "frame,villa_gain\n";
    for (int frame = 0; frame < kPvFps * kPvSeconds; ++frame) {
        const float time = static_cast<float>(frame) / kPvFps;
        const auto camera = PvCamera(time);
        listener << frame << ',' << sunset::VillaGain(camera.GetPosition()) << '\n';
        scene.Draw(renderer, camera, sunset::Detail::Detailed, time, PvDaylight(time));
        renderer.EndFrame();
        char name[24];
        std::snprintf(name, sizeof(name), "frame-%04d.ppm", frame);
        SavePpm(directory / name, renderer.ReadOffscreenRgba());
    }
    if (!listener) throw std::runtime_error("Could not write audio trajectory");
    renderer.Shutdown();
    return 0;
}

void SetTitle(HWND window, sunset::Detail detail) {
    const wchar_t* name = detail == sunset::Detail::Detailed ? L"Detailed" : L"Simple";
    const std::wstring title = std::wstring(L"Noctis | Sunset Harbor | ") + name +
        L" | WASD: Walk / Tab: Mouse / F2: Assets / R: Reset / Esc: Quit";
    SetWindowTextW(window, title.c_str());
}

int Play() {
    Window window;
    if (!window.Initialize(GetModuleHandleW(nullptr), kWidth, kHeight, L"Noctis | Sunset Harbor")) {
        throw std::runtime_error("Window initialization failed");
    }
    Renderer renderer;
    renderer.Initialize(window.GetHandle(), kWidth, kHeight);
    if (!renderer.IsPbrAvailable() || !renderer.IsShadowEnabled()) throw std::runtime_error("Missing PBR or shadow pipeline");
    sunset::Scene scene(renderer);
    Walk walk;
    auto detail = sunset::Detail::Detailed;
    SetTitle(window.GetHandle(), detail);

    auto previous = std::chrono::steady_clock::now();
    float animationTime = 0.f;
    sunset::CoastalAudio ambience;
    bool tabHeld = false;
    bool assetHeld = false;
    auto& input = window.GetInput();
    while (window.ProcessMessages()) {
        input.BeginFrame(window.GetHandle());
        if (input.IsKeyDown(VK_ESCAPE)) break;
        const auto now = std::chrono::steady_clock::now();
        const float seconds = std::min(.05f, std::chrono::duration<float>(now - previous).count());
        previous = now;
        animationTime += seconds;

        const bool tab = input.IsKeyDown(VK_TAB);
        if (tab && !tabHeld) input.SetCursorLock(!input.IsCursorLocked(), window.GetHandle());
        tabHeld = tab;

        const bool changeAsset = input.IsKeyDown(VK_F2);
        if (changeAsset && !assetHeld) {
            detail = detail == sunset::Detail::Detailed ? sunset::Detail::Simple : sunset::Detail::Detailed;
            SetTitle(window.GetHandle(), detail);
        }
        assetHeld = changeAsset;

        if (input.IsKeyDown('R')) walk = Walk{};
        if (input.IsCursorLocked()) {
            int dx = 0, dy = 0;
            input.GetMouseDelta(dx, dy);
            walk.yaw = std::remainder(walk.yaw + dx * .0023f, 6.2831853f);
            walk.pitch = std::clamp(walk.pitch - dy * .0023f, -1.30f, 1.30f);
        }

        const float strafe = static_cast<float>(input.IsKeyDown('D') - input.IsKeyDown('A'));
        const float forward = static_cast<float>(input.IsKeyDown('W') - input.IsKeyDown('S'));
        walk.Move(strafe, forward, seconds);
        const auto camera = walk.View();
        ambience.Update(camera.GetPosition());
        const float cycle = .5f - .5f * std::cos(animationTime * .028f);
        scene.Draw(renderer, camera, detail, animationTime, cycle);
        renderer.EndFrame();
    }
    input.SetCursorLock(false, window.GetHandle());
    renderer.Shutdown();
    return 0;
}
}

int wmain(int argc, wchar_t** argv) {
    EnableConsoleUtf8();
    try {
        if (argc == 1) return Play();
        const std::wstring_view operation = argv[1];
        if (argc == 2 && operation == L"--self-test") {
            SelfTest();
            return 0;
        }
        if (argc == 3 && operation == L"--capture") return Capture(argv[2]);
        if (argc == 3 && operation == L"--pv-frames") return RenderPv(argv[2]);
        throw std::runtime_error("Usage: SunsetWalk.exe [--self-test | --capture NEW_DIRECTORY | --pv-frames NEW_DIRECTORY]");
    } catch (const std::exception& error) {
        std::fprintf(stderr, "SunsetWalk: %s\n", error.what());
        return 1;
    }
}
