#pragma once

#include "game/desert/DesertScene.h"
#include "game/ScenicPeople.h"

#include <filesystem>

namespace inland {
inline constexpr float kWalkStart = -30.f;
inline constexpr float kWalkEnd = 950.f;
inline constexpr float kWalkHalfWidth = 1.9f;
inline constexpr float kGrassStation = 15.f;
inline constexpr float kForestStation = 379.f;
inline constexpr float kFactoryStation = 635.f;

DirectX::XMFLOAT2 MoveOnPath(const desert::Route& route, DirectX::XMFLOAT2 position, DirectX::XMFLOAT2 delta);

struct Shot {
    Camera camera;
    float fieldOfView;
    float dusk;
    float rain;
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
    GltfProp m_grass;
    GltfProp m_trees;
    GltfProp m_canopy;
    GltfProp m_shelter;
    GltfProp m_yard;
    GltfProp m_lamps;
    GltfProp m_sky;
    ScenicPeople m_traveler;
    ScenicPeople m_crew;
};
}
