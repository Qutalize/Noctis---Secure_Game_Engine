#include "game/sunset/SunsetScene.h"

#include "engine/core/ConsoleUtf8.h"
#include "engine/core/Window.h"
#include "engine/math/Collision.h"

#include <mmsystem.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using namespace DirectX;
namespace fs = std::filesystem;
namespace {
constexpr int kWidth = 960;
constexpr int kHeight = 540;
constexpr int kPvFps = 24;
constexpr int kPvSeconds = 12;
constexpr float kCloseTime = 10.5f;
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
        center = ResolveAABBCollision(center, kHalfBody, delta, walls);
        center.x = std::clamp(center.x, -2.55f, 2.55f);
        center.z = std::clamp(center.z, -9.7f, 15.7f);
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
    if (time < 4.5f) {
        const float amount = Ease(time / 4.5f);
        return LookAt({std::lerp(-9.6f, -8.2f, amount), std::lerp(5.2f, 4.2f, amount),
                       std::lerp(-15.4f, -12.8f, amount)}, {-.40f, 1.3f, 11.5f});
    }
    if (time < 8.5f) {
        const float amount = Ease((time - 4.5f) / 4.f);
        return LookAt({std::lerp(-2.22f, -.85f, amount), 1.71f, std::lerp(-7.75f, -4.12f, amount)},
                      {1.10f, 1.47f, 5.85f});
    }
    const float amount = Ease((time - 8.5f) / 3.5f);
    return LookAt({std::lerp(.68f, 1.25f, amount), std::lerp(1.50f, 1.37f, amount), -2.57f},
                  {3.17f, 1.21f, 1.25f});
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
    if (walk.center.z != -9.7f) throw std::runtime_error("Harbor boundary failed");
    std::puts("Sunset walk checks passed: movement, column collision, harbor boundary");
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
                scene.Draw(renderer, camera, detail, time);
                renderer.EndFrame();
                const auto time = std::chrono::duration<double, std::milli>(
                    std::chrono::steady_clock::now() - start).count();
                if (i >= kWarmup) {
                    measurements << shot << ',' << asset << ',' << scene.TriangleCount(detail) << ','
                                 << (i - kWarmup) << ',' << time << '\n';
                }
            }
            SavePpm(directory / (std::string(shot) + "-" + asset + ".ppm"), renderer.ReadOffscreenRgba());
        }
    }
    if (!measurements) throw std::runtime_error("Could not write capture samples");
    scene.Draw(renderer, PvCamera(0.f), sunset::Detail::Detailed, .62f);
    renderer.EndFrame();
    SavePpm(directory / "wide-water-later.ppm", renderer.ReadOffscreenRgba());
    renderer.Shutdown();
    return 0;
}

int RenderPv(const fs::path& directory) {
    if (!fs::create_directory(directory)) throw std::runtime_error("PV frame directory must be new");
    Renderer renderer;
    renderer.Initialize(nullptr, kWidth, kHeight);
    if (!renderer.IsPbrAvailable() || !renderer.IsShadowEnabled()) throw std::runtime_error("Missing PBR or shadow pipeline");
    sunset::Scene scene(renderer);
    for (int frame = 0; frame < kPvFps * kPvSeconds; ++frame) {
        const float time = static_cast<float>(frame) / kPvFps;
        scene.Draw(renderer, PvCamera(time), sunset::Detail::Detailed, time);
        renderer.EndFrame();
        char name[24];
        std::snprintf(name, sizeof(name), "frame-%04d.ppm", frame);
        SavePpm(directory / name, renderer.ReadOffscreenRgba());
    }
    renderer.Shutdown();
    return 0;
}

void SetTitle(HWND window, sunset::Detail detail) {
    const wchar_t* name = detail == sunset::Detail::Detailed ? L"Detailed" : L"Simple";
    const std::wstring title = std::wstring(L"Noctis | Sunset Harbor | ") + name +
        L" | WASD: Walk / Tab: Mouse / F2: Assets / R: Reset / Esc: Quit";
    SetWindowTextW(window, title.c_str());
}

class Ambience {
public:
    Ambience() {
        wchar_t executable[32768];
        const DWORD length = GetModuleFileNameW(nullptr, executable, static_cast<DWORD>(std::size(executable)));
        if (length == 0 || length >= std::size(executable)) return;
        const auto audio = fs::path(executable).parent_path() / L"sunset-ambience.wav";
        if (fs::exists(audio)) {
            m_playing = PlaySoundW(audio.c_str(), nullptr, SND_ASYNC | SND_FILENAME | SND_LOOP | SND_NODEFAULT);
        }
    }

    ~Ambience() {
        if (m_playing) PlaySoundW(nullptr, nullptr, 0);
    }

private:
    bool m_playing = false;
};

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
    const Ambience ambience;
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
        scene.Draw(renderer, walk.View(), detail, animationTime);
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
