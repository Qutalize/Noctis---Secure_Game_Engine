#include "game/inland/InlandScene.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>

using namespace DirectX;

namespace inland {
namespace {
Camera LookAt(const XMFLOAT3& eye, const XMFLOAT3& target) {
    Camera camera;
    camera.SetPosition(eye);
    const float horizontal = target.x - eye.x;
    const float depth = target.z - eye.z;
    camera.SetRotation(std::atan2(horizontal, depth),
                       std::atan2(target.y - eye.y, std::hypot(horizontal, depth)));
    return camera;
}
}

XMFLOAT2 MoveOnPath(const desert::Route& route, XMFLOAT2 position, XMFLOAT2 delta) {
    const float z = std::clamp(position.y + delta.y, kWalkStart, kWalkEnd);
    const float center = route.At(z).x;
    return {std::clamp(position.x + delta.x, center - kWalkHalfWidth, center + kWalkHalfWidth), z};
}

Shot ComposeShot(const desert::Route& route, float time) {
    const float phase = std::fmod(std::clamp(time, 0.f, 11.999f), 4.f) * .25f;
    const float travel = phase * phase * (3.f - 2.f * phase);
    if (time < 4.f) {
        const auto eye = route.Point(-13.f + 14.f * travel, 9.f - 6.f * travel, 5.2f - 2.4f * travel);
        return {LookAt(eye, route.Point(32.f + 8.f * travel, -3.f + 2.f * travel, .6f)), 45.f, .20f};
    }
    if (time < 8.f) {
        const auto eye = route.Point(58.f + 15.f * travel, -11.6f + 7.5f * travel,
                                     2.9f - 1.8f * travel + .32f * std::sin(phase * 3.14f));
        return {LookAt(eye, route.Point(84.f + 11.f * travel, 5.f + 2.f * travel, .20f)), 48.f, .39f};
    }
    const auto eye = route.Point(113.f + 14.5f * travel, 1.9f + 2.1f * travel, 2.35f + 1.5f * travel);
    return {LookAt(eye, route.Point(153.f + 7.f * travel, -9.f - 4.f * travel, 5.2f)), 43.f, .84f};
}

Scene::Scene(Renderer& renderer, const std::filesystem::path& assets) : m_route(assets / "route.json") {
    const auto load = [&](GltfProp& prop, const char* name) {
        const auto path = (assets / name / (std::string(name) + "_1k.gltf")).u8string();
        if (!prop.Load(renderer, std::string(path.begin(), path.end()), true, true)) {
            throw std::runtime_error(std::string("Missing inland glTF: ") + name);
        }
    };
    load(m_ground, "inland_ground");
    load(m_path, "inland_path");
    load(m_grass, "inland_grasses");
    load(m_water, "inland_water");
    load(m_yard, "inland_yard");
    load(m_lamps, "inland_lamps");
    load(m_sky, "inland_sky");
}

void Scene::Draw(Renderer& renderer, const Shot& shot, float time) {
    const float dusk = shot.dusk;
    const float elevation = std::lerp(.54f, .027f, dusk);
    const float azimuth = std::lerp(-1.10f, -.43f, dusk);
    const XMFLOAT3 sun{std::sin(azimuth) * std::cos(elevation), std::sin(elevation),
                      std::cos(azimuth) * std::cos(elevation)};
    const float afterglow = std::lerp(.07f, .82f, dusk);
    const float night = std::lerp(0.f, .58f, dusk * dusk);
    const float lamps = std::clamp((dusk - .34f) * 1.9f, 0.f, 1.f);
    renderer.SetFog(false);
    renderer.SetDirectionalLight({-sun.x, -sun.y, -sun.z}, {1.f, .80f, .59f},
                                 std::lerp(1.82f, .55f, dusk), {.49f, .55f, .64f});
    renderer.SetCoastalHaze({.45f, .50f, .55f}, .00175f);
    renderer.SetShadowExtent(52.f);
    renderer.SetWarmFillLight({-4.1f, 4.9f, 115.f}, lamps * 2.45f);
    renderer.SetWarmAccentLights({6.7f, 4.85f, 144.f, lamps * 2.15f}, {-16.f, 4.9f, 118.4f, lamps * 2.25f});
    renderer.BeginFrame(shot.camera, shot.fieldOfView);

    const auto& eye = shot.camera.GetPosition();
    m_sky.SetScenicEffect({2.f, time, afterglow, night});
    m_grass.SetScenicEffect({-3.f, time, 0.f, 0.f});
    m_water.SetScenicEffect({4.f, time, afterglow, night});
    m_lamps.SetScenicEffect({3.f, time, 0.f, lamps});
    m_sky.Draw(renderer, XMMatrixTranslation(eye.x, eye.y, eye.z), true);
    const auto identity = XMMatrixIdentity();
    m_ground.Draw(renderer, identity);
    m_water.Draw(renderer, identity);
    m_path.Draw(renderer, identity);
    m_grass.Draw(renderer, identity);
    m_yard.Draw(renderer, identity);
    m_lamps.Draw(renderer, identity);
}
}
