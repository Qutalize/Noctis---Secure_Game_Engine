#pragma once
#include "engine/renderer/Renderer.h"
#include "game/GltfProp.h"
#include <cstddef>

namespace sunset {
enum class Detail { Simple, Detailed };

class Scene {
public:
    explicit Scene(Renderer& renderer);
    void Draw(Renderer& renderer, const Camera& camera, Detail detail, float time, float daylight);
    std::size_t TriangleCount(Detail detail) const;

private:
    struct Buildings {
        Mesh geometry;
        PbrMaterial material;
        std::size_t triangles = 0;
    };

    Mesh m_sky;
    PbrMaterial m_skyMaterial;
    Mesh m_islands;
    Mesh m_ruinIsland;
    Mesh m_shore;
    GltfProp m_ruins;
    GltfProp m_boardwalk;
    GltfProp m_villa;
    Mesh m_foreshore;
    PbrMaterial m_sandMaterial;
    Mesh m_restaurant;
    GltfProp m_rock;
    GltfProp m_diningSet;
    GltfProp m_chandelier;
    Mesh m_lanterns;
    PbrMaterial m_lanternMaterial;
    Mesh m_water;
    PbrMaterial m_waterMaterial;
    Mesh m_resort;
    Mesh m_boat;
    Mesh m_leftWing;
    Mesh m_rightWing;
    Buildings m_simple;
    Buildings m_detailed;
};
}
