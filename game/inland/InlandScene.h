#pragma once

#include "game/desert/DesertScene.h"

#include <filesystem>

namespace inland {
inline constexpr float kWalkStart = -20.f;
inline constexpr float kWalkEnd = 180.f;
inline constexpr float kWalkHalfWidth = 1.9f;

DirectX::XMFLOAT2 MoveOnPath(const desert::Route& route, DirectX::XMFLOAT2 position, DirectX::XMFLOAT2 delta);

struct Shot {
    Camera camera;
    float fieldOfView;
    float dusk;
};

Shot ComposeShot(const desert::Route& route, float time);

class Scene {
public:
    Scene(Renderer& renderer, const std::filesystem::path& assets);
    void Draw(Renderer& renderer, const Shot& shot, float time);
    const desert::Route& GetRoute() const { return m_route; }

private:
    desert::Route m_route;
    GltfProp m_ground;
    GltfProp m_path;
    GltfProp m_grass;
    GltfProp m_water;
    GltfProp m_yard;
    GltfProp m_lamps;
    GltfProp m_sky;
};
}
