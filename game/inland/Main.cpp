#include "game/inland/InlandScene.h"
#include "engine/core/ConsoleUtf8.h"
#include "engine/core/Window.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <numbers>
#include <stdexcept>
#include <string_view>

using namespace DirectX;
namespace fs = std::filesystem;

namespace {
constexpr int kWidth = 960, kHeight = 540, kFps = 24, kFrames = 12 * kFps;
constexpr float kEyeHeight = 1.63f;

fs::path AssetDirectory() {
    wchar_t executable[32768]{};
    const auto length = GetModuleFileNameW(nullptr, executable, static_cast<DWORD>(std::size(executable)));
    if (!length || length >= std::size(executable)) throw std::runtime_error("Cannot locate InlandTour");
    return fs::path(executable).parent_path() / "inland-assets";
}

void SelfTest(const desert::Route& route) {
    auto position = inland::MoveOnPath(route, {}, {0.f, -100.f});
    if (position.y != inland::kWalkStart) throw std::runtime_error("Invalid start boundary");
    for (float z = position.y; z < inland::kWalkEnd; z += .25f) {
        const auto next = inland::MoveOnPath(route, position, {10.f, .25f});
        const auto previous = route.At(position.y);
        const auto current = route.At(next.y);
        if (next.y - position.y != .25f || std::abs(next.x - current.x - inland::kWalkHalfWidth) > .001f ||
            std::abs(current.y - previous.y) > .05f || current.y < .57f) {
            throw std::runtime_error("Invalid footpath or boardwalk");
        }
        position = next;
    }
    if (inland::MoveOnPath(route, position, {-100.f, 100.f}).y != inland::kWalkEnd) {
        throw std::runtime_error("Invalid end boundary");
    }
    for (int i = 0; i < kFrames; ++i) {
        const auto shot = inland::ComposeShot(route, static_cast<float>(i) / kFps);
        const auto eye = shot.camera.GetPosition();
        if (eye.y < 1.5f || !std::isfinite(shot.camera.GetYaw()) || !std::isfinite(shot.camera.GetPitch())) {
            throw std::runtime_error("Invalid inland camera");
        }
    }
    std::puts("Inland checks passed: 800 walking steps, boundaries and 288 camera positions");
}

void RequirePbr(const Renderer& renderer) {
    if (!renderer.IsPbrAvailable() || !renderer.IsShadowEnabled()) throw std::runtime_error("Missing PBR or shadows");
}

void SavePpm(const fs::path& file, const std::vector<uint8_t>& rgba) {
    if (rgba.size() != static_cast<std::size_t>(kWidth * kHeight * 4)) throw std::runtime_error("Invalid inland frame");
    std::vector<uint8_t> rgb(kWidth * kHeight * 3);
    for (std::size_t pixel = 0; pixel < rgb.size() / 3; ++pixel) {
        std::copy_n(rgba.data() + pixel * 4, 3, rgb.data() + pixel * 3);
    }
    std::ofstream stream(file, std::ios::binary);
    stream << "P6\n" << kWidth << ' ' << kHeight << "\n255\n";
    stream.write(reinterpret_cast<const char*>(rgb.data()), static_cast<std::streamsize>(rgb.size()));
    if (!stream) throw std::runtime_error("Cannot save InlandTour frame");
}

int Capture(const fs::path& directory) {
    if (!fs::create_directory(directory)) throw std::runtime_error("Capture directory must be new");
    Renderer renderer;
    renderer.Initialize(nullptr, kWidth, kHeight, false, true);
    RequirePbr(renderer);
    if (renderer.SampleCount() != 4) throw std::runtime_error("WARP did not provide 4x MSAA");
    inland::Scene scene(renderer, AssetDirectory());
    for (int index = 0; index < kFrames; ++index) {
        const float time = static_cast<float>(index) / kFps;
        scene.Draw(renderer, inland::ComposeShot(scene.GetRoute(), time), time);
        renderer.EndFrame();
        char name[24]{};
        std::snprintf(name, sizeof(name), "frame-%04d.ppm", index);
        SavePpm(directory / name, renderer.ReadOffscreenRgba());
    }
    renderer.Shutdown();
    return 0;
}

int Play() {
    Window window;
    if (!window.Initialize(GetModuleHandleW(nullptr), kWidth, kHeight, L"Noctis | Inland Tour")) {
        throw std::runtime_error("Window initialization failed");
    }
    Renderer renderer;
    renderer.Initialize(window.GetHandle(), kWidth, kHeight, false, true);
    RequirePbr(renderer);
    inland::Scene scene(renderer, AssetDirectory());
    const auto& route = scene.GetRoute();
    SetWindowTextW(window.GetHandle(), renderer.SampleCount() == 4
        ? L"Noctis | Inland Tour | 4x MSAA | WASD / Shift / Tab / 1-3"
        : L"Noctis | Inland Tour | AA off | WASD / Shift / Tab / 1-3");
    auto start = route.At(inland::kWalkStart);
    XMFLOAT2 position{start.x, inland::kWalkStart};
    float yaw = 0.f, pitch = 0.f, time = 0.f;
    bool tabHeld = false;
    auto previous = std::chrono::steady_clock::now();
    auto& input = window.GetInput();
    while (window.ProcessMessages()) {
        input.BeginFrame(window.GetHandle());
        if (input.IsKeyDown(VK_ESCAPE)) break;
        const auto now = std::chrono::steady_clock::now();
        const float elapsed = std::min(.05f, std::chrono::duration<float>(now - previous).count());
        previous = now;
        time += elapsed;
        const bool tab = input.IsKeyDown(VK_TAB);
        if (tab && !tabHeld) input.SetCursorLock(!input.IsCursorLocked(), window.GetHandle());
        tabHeld = tab;
        constexpr float stations[]{-13.f, 62.f, 115.f};
        for (int index = 0; index < static_cast<int>(std::size(stations)); ++index) {
            if (input.IsKeyDown('1' + index)) {
                const float z = stations[index];
                position = {route.At(z).x, z};
                yaw = index == 2 ? -.20f : .10f;
                pitch = 0.f;
            }
        }
        if (input.IsCursorLocked()) {
            int dx = 0, dy = 0;
            input.GetMouseDelta(dx, dy);
            yaw = std::remainder(yaw + dx * .0023f, 2.f * std::numbers::pi_v<float>);
            pitch = std::clamp(pitch - dy * .0023f, -1.3f, 1.3f);
        }
        const float strafe = static_cast<float>(input.IsKeyDown('D') - input.IsKeyDown('A'));
        const float forward = static_cast<float>(input.IsKeyDown('W') - input.IsKeyDown('S'));
        const float step = elapsed * (input.IsKeyDown(VK_SHIFT) ? 9.f : 3.5f) /
                           std::max(1.f, std::hypot(strafe, forward));
        position = inland::MoveOnPath(route, position, {
            step * (strafe * std::cos(yaw) + forward * std::sin(yaw)),
            step * (forward * std::cos(yaw) - strafe * std::sin(yaw))});
        Camera camera;
        camera.SetPosition({position.x, route.At(position.y).y + kEyeHeight, position.y});
        camera.SetRotation(yaw, pitch);
        const float dusk = std::lerp(.20f, .86f, std::clamp((position.y - 28.f) / 94.f, 0.f, 1.f));
        scene.Draw(renderer, {camera, 57.f, dusk}, time);
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
        const std::wstring_view command = argv[1];
        if (argc == 2 && command == L"--self-test") {
            SelfTest(desert::Route(AssetDirectory() / "route.json"));
            return 0;
        }
        if (argc == 3 && command == L"--capture") return Capture(argv[2]);
        throw std::runtime_error("Usage: InlandTour.exe [--self-test | --capture NEW_DIRECTORY]");
    } catch (const std::exception& error) {
        std::fprintf(stderr, "InlandTour: %s\n", error.what());
        return 1;
    }
}
