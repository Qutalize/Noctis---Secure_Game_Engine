#pragma once

#include "game/GltfProp.h"

#include <filesystem>

namespace desert {
class Route {
public:
    explicit Route(const std::filesystem::path& file);
    DirectX::XMFLOAT2 At(float distance) const;
    DirectX::XMFLOAT3 Point(float distance, float offset, float height) const;
    DirectX::XMFLOAT2 Step(DirectX::XMFLOAT2 position, DirectX::XMFLOAT2 delta) const;

private:
    std::vector<DirectX::XMFLOAT3> m_keys;
};

class Scene {
public:
    Scene(Renderer& renderer, const std::filesystem::path& assets);
    void Draw(Renderer& renderer, const Camera& camera, float time, float evening, float verticalFov);
    const Route& GetRoute() const { return m_route; }

private:
    Route m_route;
    GltfProp m_ground;
    GltfProp m_mesas;
    GltfProp m_props;
    GltfProp m_rock;
    Mesh m_sky;
    PbrMaterial m_skyMaterial;
};
}
