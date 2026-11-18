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
        const float descent = std::clamp(phase * 1.85f, 0.f, 1.f);
        const auto eye = route.Point(-11.f + 50.f * travel, -5.1f + 9.2f * travel,
                                     std::lerp(10.f, 1.42f, descent * descent * (3.f - 2.f * descent)));
        return {LookAt(eye, route.Point(75.f + 32.f * travel, -10.f + 6.f * travel, .23f)), 54.f, .14f, 0.f};
    }
    if (time < 8.f) {
        const auto eye = route.Point(374.f + 18.f * travel, -1.35f + 2.8f * travel, 1.72f + .46f * travel);
        return {LookAt(eye, route.Point(405.f + 18.f * travel, 5.2f - 4.4f * travel, 3.3f)), 47.f, .31f, 1.f};
    }
    const auto eye = route.Point(627.f + 26.f * travel, 2.6f + 1.2f * travel, 1.6f + 3.3f * travel);
    return {LookAt(eye, route.Point(700.f + 32.f * travel, 1.4f - 7.f * travel, 9.2f)), 44.f, .84f, 0.f};
}

Scene::Scene(Renderer& renderer, const std::filesystem::path& assets) : m_route(assets / "route.json") {
    const auto load = [&](GltfProp& prop, const char* name) {
        const auto path = (assets / name / (std::string(name) + "_1k.gltf")).u8string();
        if (!prop.Load(renderer, std::string(path.begin(), path.end()), true, true)) {
            throw std::runtime_error(std::string("Missing inland glTF: ") + name);
        }
    };
    load(m_meadow, "inland_meadow");
    load(m_ground, "inland_ground");
    load(m_grass, "inland_grasses");
    load(m_trees, "inland_trees");
    load(m_canopy, "inland_canopy");
    load(m_shelter, "inland_shelter");
    load(m_yard, "inland_yard");
    load(m_lamps, "inland_lamps");
    load(m_sky, "inland_sky");
    m_canopy.SetScenicEffect({-5.f, 0.f, 0.f, 0.f});
    m_ground.SetScenicEffect({-4.f, 0.f, 0.f, 0.f});
    m_meadow.SetScenicEffect({-7.f, 0.f, 0.f, 0.f});
    m_traveler.Load(renderer, assets, "sage");
    m_crew.Load(renderer, assets, "colleagues");
}

void Scene::Draw(Renderer& renderer, const Shot& shot, float time) {
    const float dusk = shot.dusk;
    const float rain = shot.rain;
    const float elevation = std::lerp(.54f, .027f, dusk);
    const float azimuth = std::lerp(-1.10f, -.43f, dusk);
    const XMFLOAT3 sun{std::sin(azimuth) * std::cos(elevation), std::sin(elevation),
                      std::cos(azimuth) * std::cos(elevation)};
    const float afterglow = std::lerp(.07f, .82f, dusk);
    const float night = std::lerp(0.f, .58f, dusk * dusk);
    const float lamps = std::clamp((dusk - .34f) * 1.9f, 0.f, 1.f);
    renderer.SetFog(false);
    renderer.SetDirectionalLight({-sun.x, -sun.y, -sun.z}, {1.f, std::lerp(.86f,.97f,rain), std::lerp(.68f,1.f,rain)},
                                 std::lerp(std::lerp(2.8f, .59f, dusk), .24f, rain),
                                 {std::lerp(.66f,.31f,rain),std::lerp(.72f,.39f,rain),std::lerp(.73f,.43f,rain)});
    renderer.SetCoastalHaze({std::lerp(.52f,.32f,rain), std::lerp(.60f,.40f,rain), std::lerp(.68f,.44f,rain)},
                            std::lerp(.00142f,.024f,rain));
    renderer.SetScenicRain(rain, time);
    renderer.SetShadowExtent(78.f);
    renderer.SetWarmFillLight({-4.1f, 4.85f, 635.f}, lamps * 13.9f);
    renderer.SetWarmAccentLights({-4.1f, 4.85f, 664.f, lamps * 12.f}, {6.7f, 4.85f, 664.f, lamps * 12.f});
    renderer.BeginFrame(shot.camera, shot.fieldOfView, 1500.f);

    const auto& eye = shot.camera.GetPosition();
    m_sky.SetScenicEffect({2.f, time, afterglow, night});
    m_grass.SetScenicEffect({-3.f, time, 0.f, 0.f});
    m_lamps.SetScenicEffect({3.f, time, 0.f, lamps});
    m_sky.Draw(renderer, XMMatrixTranslation(eye.x, eye.y, eye.z), true);
    const auto identity = XMMatrixIdentity();
    m_meadow.Draw(renderer, identity);
    m_ground.Draw(renderer, identity);
    m_grass.Draw(renderer, identity);
    m_trees.Draw(renderer, identity);
    m_canopy.Draw(renderer, identity);
    m_shelter.Draw(renderer, identity);
    m_yard.Draw(renderer, identity);
    m_lamps.Draw(renderer, identity);
    m_traveler.Draw(renderer, shot.camera, m_route.Point(46.f, .65f, -.025f), .36f);
    m_traveler.Draw(renderer, shot.camera, m_route.Point(404.f, 1.2f, -.025f), .48f);
    m_crew.Draw(renderer, shot.camera, {-1.7f, 1.015f, 660.f}, -.32f);
    m_crew.Draw(renderer, shot.camera, {15.2f, 1.015f, 754.f}, .58f);
}
}
