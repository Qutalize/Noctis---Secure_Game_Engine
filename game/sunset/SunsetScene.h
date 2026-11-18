#pragma once
#include "engine/renderer/Renderer.h"
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
    Mesh m_shore;
    Mesh m_ruins;
    Mesh m_boardwalk;
    Mesh m_restaurant;
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
