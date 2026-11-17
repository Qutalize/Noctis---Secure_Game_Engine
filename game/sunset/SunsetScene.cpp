#include "game/sunset/SunsetScene.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>
#include <vector>

using namespace DirectX;

namespace sunset {
namespace {
constexpr float kPi = std::numbers::pi_v<float>;
const XMFLOAT3 kSun{-0.35f, 0.16f, 0.923f};

float Mix(float a, float b, float amount) {
    return a + (b - a) * amount;
}

float Pattern(float x, float y) {
    return .5f + .25f * std::sin(x * 13.4f + y * 31.1f) +
        .25f * std::sin(x * 41.8f - y * 19.9f);
}

XMFLOAT4 SkyColor(const XMFLOAT3& direction) {
    const float height = std::clamp(direction.y, 0.f, 1.f);
    const float upper = 1.f - std::exp(-height * 4.f);
    const float alignment = std::clamp(direction.x * kSun.x + direction.y * kSun.y +
                                       direction.z * kSun.z, 0.f, 1.f);
    const float glow = std::pow(alignment, 36.f) * .44f;
    const float disk = std::clamp((alignment - .99956f) / .00026f, 0.f, 1.f);
    return {Mix(.78f, .16f, upper) + glow + disk,
            Mix(.42f, .28f, upper) + glow * .48f + disk * .78f,
            Mix(.33f, .46f, upper) + glow * .10f + disk * .42f, 1.f};
}

Mesh Sky(Renderer& renderer) {
    constexpr int rings = 72;
    constexpr int segments = 144;
    constexpr float radius = 165.f;
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    for (int y = 0; y <= rings; ++y) {
        const float elevation = Mix(-.22f, kPi * .5f, static_cast<float>(y) / rings);
        for (int x = 0; x <= segments; ++x) {
            const float azimuth = 2.f * kPi * static_cast<float>(x) / segments;
            const XMFLOAT3 direction{std::sin(azimuth) * std::cos(elevation),
                                     std::sin(elevation), std::cos(azimuth) * std::cos(elevation)};
            vertices.push_back({{radius * direction.x, radius * direction.y, radius * direction.z},
                                kSun, SkyColor(direction)});
            if (x < segments && y < rings) {
                const auto index = static_cast<uint32_t>(y * (segments + 1) + x);
                const auto next = index + segments + 1;
                indices.insert(indices.end(), {index, next, index + 1, index + 1, next, next + 1});
            }
        }
    }
    return renderer.CreateMesh(vertices, indices);
}

Mesh Islands(Renderer& renderer) {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    for (int layer = 0; layer < 2; ++layer) {
        constexpr int samples = 100;
        const auto start = static_cast<uint32_t>(vertices.size());
        const XMFLOAT4 color = layer == 0 ? XMFLOAT4{.48f, .34f, .41f, 1.f}
                                          : XMFLOAT4{.34f, .25f, .33f, 1.f};
        for (int i = 0; i <= samples; ++i) {
            const float x = Mix(-120.f, 120.f, static_cast<float>(i) / samples);
            const float envelope = std::exp(-std::pow((x - 43.f + layer * 10.f) / 34.f, 2.f)) +
                                   .7f * std::exp(-std::pow((x + 83.f) / 14.f, 2.f));
            const float top = envelope * (2.2f + 6.f * Pattern(x * .013f, layer * .3f)) - 1.4f;
            const float z = 116.f - layer * 19.f;
            vertices.push_back({{x, -2.f, z}, kSun, color});
            vertices.push_back({{x, top, z}, kSun, color});
            if (i < samples) {
                const auto base = start + static_cast<uint32_t>(2 * i);
                indices.insert(indices.end(), {base, base + 1, base + 2, base + 2, base + 1, base + 3});
            }
        }
    }
    return renderer.CreateMesh(vertices, indices);
}

struct MeshData {
    std::vector<PbrVertex> vertices;
    std::vector<uint32_t> indices;

    void Quad(const std::array<XMFLOAT3, 4>& corners, bool worldUv = false) {
        const XMVECTOR first = XMLoadFloat3(&corners[0]);
        const XMVECTOR tangent = XMVector3Normalize(XMLoadFloat3(&corners[1]) - first);
        const XMVECTOR normal = XMVector3Normalize(XMVector3Cross(
            XMLoadFloat3(&corners[2]) - first, tangent));
        XMFLOAT3 normalValue;
        XMFLOAT4 tangentValue;
        XMStoreFloat3(&normalValue, normal);
        XMStoreFloat4(&tangentValue, tangent);
        // Flip the bitangent to agree with corners 0,1,2,3 and their UVs.
        tangentValue.w = -1.f;
        constexpr XMFLOAT2 uv[] = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};
        const auto base = static_cast<uint32_t>(vertices.size());
        for (std::size_t i = 0; i < corners.size(); ++i) {
            const XMFLOAT2 coords = worldUv ? XMFLOAT2{corners[i].x * .35f, corners[i].z * .35f} : uv[i];
            vertices.push_back({corners[i], normalValue, tangentValue, coords});
        }
        indices.insert(indices.end(), {base, base + 1, base + 2, base, base + 2, base + 3});
    }
};

void StoneBox(MeshData& mesh, float x, float y, float z, float width, float height, float length) {
    const float right = x + width;
    const float top = y + height;
    const float back = z + length;
    mesh.Quad({{{x, y, z}, {right, y, z}, {right, top, z}, {x, top, z}}});
    mesh.Quad({{{right, y, back}, {x, y, back}, {x, top, back}, {right, top, back}}});
    mesh.Quad({{{x, y, back}, {x, y, z}, {x, top, z}, {x, top, back}}});
    mesh.Quad({{{right, y, z}, {right, y, back}, {right, top, back}, {right, top, z}}});
    mesh.Quad({{{x, top, z}, {right, top, z}, {right, top, back}, {x, top, back}}});
}

void Arch(MeshData& mesh, float x, float z, bool detailed) {
    constexpr float spring = 2.26f;
    constexpr float inner = 1.18f;
    constexpr float outer = 1.61f;
    constexpr float depth = .74f;
    const int courses = detailed ? 7 : 1;
    constexpr float pillarWidth = .59f;
    for (int side : {-1, 1}) {
        const float center = x + side * (inner + .19f);
        for (int course = 0; course < courses; ++course) {
            const float joint = detailed ? .018f : 0.f;
            StoneBox(mesh, center - pillarWidth / 2.f, course * spring / courses, z,
                     pillarWidth, spring / courses - joint, depth);
        }
        if (detailed) {
            StoneBox(mesh, center - .38f, 0.f, z - .07f, .76f, .20f, .88f);
            StoneBox(mesh, center - .35f, spring - .15f, z - .05f, .70f, .15f, .84f);
        }
    }

    const int stones = detailed ? 24 : 8;
    const float mortar = detailed ? .009f : 0.f;
    auto point = [=](float angle, float radius, float positionZ) {
        return XMFLOAT3{x + std::cos(angle) * radius, spring + std::sin(angle) * radius, positionZ};
    };
    for (int i = 0; i < stones; ++i) {
        const float start = i * kPi / stones + mortar;
        const float end = (i + 1) * kPi / stones - mortar;
        const auto a = point(start, inner, z);
        const auto b = point(end, inner, z);
        const auto c = point(end, outer, z);
        const auto d = point(start, outer, z);
        const auto e = point(start, inner, z + depth);
        const auto f = point(end, inner, z + depth);
        const auto g = point(end, outer, z + depth);
        const auto h = point(start, outer, z + depth);
        mesh.Quad({d, c, b, a});
        mesh.Quad({e, f, g, h});
        mesh.Quad({a, b, f, e});
        mesh.Quad({d, h, g, c});
        if (detailed) {
            mesh.Quad({a, e, h, d});
            mesh.Quad({b, c, g, f});
        }
    }
}

MeshData Harbor(bool detailed) {
    MeshData data;
    StoneBox(data, -3.05f, -.68f, -10.f, 6.10f, .67f, 26.f);
    if (detailed) {
        for (int z = -10; z < 16; ++z) {
            const float shift = z % 2 == 0 ? .0f : .2f;
            for (int x = 0; x < 7; ++x) {
                StoneBox(data, -2.98f + x * .84f + shift, -.014f, static_cast<float>(z) + .04f,
                         .80f, .019f, .92f);
            }
        }
    }
    for (const float position : {1.f, 5.4f, 9.8f}) {
        Arch(data, .28f, position, detailed);
        StoneBox(data, -3.06f, -.01f, position, .35f, .66f, 3.25f);
        StoneBox(data, 2.72f, -.01f, position, .34f, .87f, .52f);
        if (detailed) {
            StoneBox(data, -3.11f, .66f, position, .46f, .085f, 3.25f);
            StoneBox(data, 2.66f, .86f, position - .05f, .46f, .10f, .62f);
        }
    }
    StoneBox(data, 1.85f, -.55f, 13.f, 1.2f, 3.8f, 2.8f);
    if (detailed) {
        for (int i = 0; i < 8; ++i) {
            StoneBox(data, 1.82f, i * .43f, 12.94f, 1.26f, .025f, 2.9f);
        }
    }
    return data;
}

Texture StoneTexture(Renderer& renderer) {
    constexpr unsigned edge = 128;
    std::vector<uint8_t> pixels(edge * edge * 4);
    for (unsigned y = 0; y < edge; ++y) {
        for (unsigned x = 0; x < edge; ++x) {
            const float u = static_cast<float>(x) / edge;
            const float v = static_cast<float>(y) / edge;
            const float grain = .69f + .22f * Pattern(u, v) + .09f * Pattern(u * 7.f, v * 11.f);
            const auto at = (y * edge + x) * 4;
            pixels[at] = static_cast<uint8_t>(255.f * grain);
            pixels[at + 1] = static_cast<uint8_t>(244.f * grain);
            pixels[at + 2] = static_cast<uint8_t>(224.f * grain);
            pixels[at + 3] = 255;
        }
    }
    return renderer.CreateTexture(edge, edge, pixels.data());
}

Texture WaterNormals(Renderer& renderer) {
    constexpr unsigned edge = 128;
    std::vector<uint8_t> pixels(edge * edge * 4);
    for (unsigned y = 0; y < edge; ++y) {
        for (unsigned x = 0; x < edge; ++x) {
            const float u = static_cast<float>(x) * 2.f * kPi / edge;
            const float v = static_cast<float>(y) * 2.f * kPi / edge;
            const float dx = .19f * std::cos(2.f * u + 5.f * v) + .10f * std::cos(u - 7.f * v);
            const float dy = .28f * std::cos(u + 6.f * v) + .11f * std::sin(4.f * u - 3.f * v);
            const auto at = (y * edge + x) * 4;
            pixels[at] = static_cast<uint8_t>(127.5f * (1.f + dx));
            pixels[at + 1] = static_cast<uint8_t>(127.5f * (1.f + dy));
            pixels[at + 2] = static_cast<uint8_t>(127.5f * (1.f + std::sqrt(1.f - dx * dx - dy * dy)));
            pixels[at + 3] = 255;
        }
    }
    return renderer.CreateTexture(edge, edge, pixels.data());
}
}

Scene::Scene(Renderer& renderer) {
    m_sky = Sky(renderer);
    m_islands = Islands(renderer);

    MeshData water;
    water.Quad({{{-145.f, -.70f, -55.f}, {145.f, -.70f, -55.f},
                 {145.f, -.70f, 124.f}, {-145.f, -.70f, 124.f}}}, true);
    m_water = renderer.CreatePbrMesh(water.vertices, water.indices);
    const auto waterNormals = WaterNormals(renderer);
    m_waterMaterial = renderer.CreatePbrMaterial(nullptr, &waterNormals, nullptr);
    m_waterMaterial.baseColorFactor = {.082f, .23f, .29f, 1.f};
    m_waterMaterial.roughnessFactor = .19f;
    m_waterMaterial.metallicFactor = 0.f;

    const auto stoneTexture = StoneTexture(renderer);
    for (const auto detail : {Detail::Simple, Detail::Detailed}) {
        const bool detailed = detail == Detail::Detailed;
        const auto data = Harbor(detailed);
        auto& buildings = detailed ? m_detailed : m_simple;
        buildings.geometry = renderer.CreatePbrMesh(data.vertices, data.indices);
        buildings.material = renderer.CreatePbrMaterial(detailed ? &stoneTexture : nullptr, nullptr, nullptr);
        buildings.material.baseColorFactor = detailed ? XMFLOAT4{.60f, .53f, .46f, 1.f}
                                                      : XMFLOAT4{.47f, .40f, .33f, 1.f};
        buildings.material.roughnessFactor = .69f;
        buildings.material.metallicFactor = 0.f;
        buildings.triangles = data.indices.size() / 3;
    }
}

void Scene::Draw(Renderer& renderer, const Camera& camera, Detail detail) const {
    const auto identity = XMMatrixIdentity();
    renderer.SetFog(false);
    renderer.SetDirectionalLight({-kSun.x, -kSun.y, -kSun.z}, {1, .71f, .43f}, 1.55f,
                                 {.33f, .36f, .49f});
    renderer.BeginFrame(camera);
    renderer.DrawMesh(m_sky, identity);
    renderer.DrawMesh(m_islands, identity);
    renderer.DrawPbrMesh(m_water, m_waterMaterial, identity);
    const auto& buildings = detail == Detail::Detailed ? m_detailed : m_simple;
    renderer.DrawPbrMesh(buildings.geometry, buildings.material, identity);
}

std::size_t Scene::TriangleCount(Detail detail) const {
    return detail == Detail::Detailed ? m_detailed.triangles : m_simple.triangles;
}
}
