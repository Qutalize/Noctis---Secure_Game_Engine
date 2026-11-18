#pragma once
#include "engine/renderer/Renderer.h"
#include <cstddef>

namespace sunset {
enum class Detail { Simple, Detailed };

class Scene {
public:
    explicit Scene(Renderer& renderer);
    void Draw(Renderer& renderer, const Camera& camera, Detail detail, float time);
    std::size_t TriangleCount(Detail detail) const;

private:
    struct Buildings {
        Mesh geometry;
        PbrMaterial material;
        std::size_t triangles = 0;
    };

    Mesh m_sky;
    Mesh m_islands;
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
