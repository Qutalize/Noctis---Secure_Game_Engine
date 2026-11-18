#include "game/sunset/SunsetScene.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <initializer_list>
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

float CloudNoise(float x, float y) {
    const float cellX = std::floor(x);
    const float cellY = std::floor(y);
    const auto sample = [](float column, float row) {
        const float value = 4375.854f * std::sin(column * 12.9898f + row * 78.233f);
        return value - std::floor(value);
    };
    const float u = x - cellX;
    const float v = y - cellY;
    const float blendX = u * u * (3.f - 2.f * u);
    const float blendY = v * v * (3.f - 2.f * v);
    return Mix(Mix(sample(cellX, cellY), sample(cellX + 1.f, cellY), blendX),
               Mix(sample(cellX, cellY + 1.f), sample(cellX + 1.f, cellY + 1.f), blendX), blendY);
}

XMFLOAT4 SkyColor(const XMFLOAT3& direction) {
    const float height = std::clamp(direction.y, 0.f, 1.f);
    const float upper = 1.f - std::exp(-height * 4.f);
    const float alignment = std::clamp(direction.x * kSun.x + direction.y * kSun.y +
                                       direction.z * kSun.z, 0.f, 1.f);
    const float glow = std::pow(alignment, 33.f) * .34f;
    const float halo = std::pow(alignment, 280.f) * .16f;
    const float spread = std::max(.10f, height + .16f);
    const float cloudX = (direction.x + .27f * direction.z) / spread * 3.4f + 4.7f;
    const float cloudY = direction.z / spread * 9.5f;
    const float density = .61f * CloudNoise(cloudX, cloudY) +
        .27f * CloudNoise(cloudX * 2.2f, cloudY * 2.2f) +
        .12f * CloudNoise(cloudX * 4.3f, cloudY * 4.3f);
    const float cloud = std::clamp((density - .51f) * 4.f, 0.f, .86f) *
        std::clamp((height - .028f) * 12.f, 0.f, 1.f);
    const float rim = .25f * std::pow(alignment, 11.f) + .17f * (1.f - cloud);
    return {Mix(Mix(.79f, .18f, upper) + glow + halo, .30f + rim, cloud),
            Mix(Mix(.44f, .33f, upper) + glow * .59f + halo * .80f, .30f + rim * .57f, cloud),
            Mix(Mix(.30f, .56f, upper) + glow * .22f + halo * .44f, .42f + rim * .27f, cloud), 1.f};
}

Mesh Sky(Renderer& renderer) {
    constexpr int rings = 126;
    constexpr int segments = 256;
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
    constexpr int diskSegments = 32;
    constexpr float diskRadius = 1.95f;
    const XMVECTOR center = XMVector3Normalize(XMLoadFloat3(&kSun)) * (radius - 1.f);
    const XMVECTOR right = XMVector3Normalize(XMVector3Cross(XMVectorSet(0, 1, 0, 0), center));
    const XMVECTOR up = XMVector3Normalize(XMVector3Cross(center, right));
    const XMFLOAT4 diskColor{1.45f, 1.13f, .66f, 1.f};
    XMFLOAT3 position;
    XMStoreFloat3(&position, center);
    const auto diskIndex = static_cast<uint32_t>(vertices.size());
    vertices.push_back({position, kSun, diskColor});
    for (int i = 0; i < diskSegments; ++i) {
        const float angle = 2.f * kPi * static_cast<float>(i) / diskSegments;
        XMStoreFloat3(&position, center + diskRadius * (std::cos(angle) * right + std::sin(angle) * up));
        vertices.push_back({position, kSun, diskColor});
        indices.insert(indices.end(), {diskIndex, diskIndex + 1 + static_cast<uint32_t>(i),
                                       diskIndex + 1 + static_cast<uint32_t>((i + 1) % diskSegments)});
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

    void Quad(const std::array<XMFLOAT3, 4>& corners) {
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
        const XMVECTOR bitangent = XMVector3Cross(normal, tangent) * -1.f;
        const float offset = std::sin(corners[0].x * 19.5f + corners[0].z * 13.3f + corners[0].y * 5.8f);
        const auto base = static_cast<uint32_t>(vertices.size());
        for (const auto& corner : corners) {
            const XMVECTOR delta = XMLoadFloat3(&corner) - first;
            const XMFLOAT2 uv{offset + XMVectorGetX(XMVector3Dot(delta, tangent)) * .48f,
                              offset + XMVectorGetX(XMVector3Dot(delta, bitangent)) * .48f};
            vertices.push_back({corner, normalValue, tangentValue, uv});
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

void ArchRing(MeshData& mesh, float x, float z, float spring, float inner, float outer,
              float depth, int stones, float mortar, bool endFaces) {
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
        if (endFaces) {
            mesh.Quad({a, e, h, d});
            mesh.Quad({b, c, g, f});
        }
    }
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
            StoneBox(mesh, center - .25f, 0.f, z + .046f, .50f, spring, depth - .092f);
            StoneBox(mesh, center - .38f, 0.f, z - .07f, .76f, .20f, .88f);
            StoneBox(mesh, center - .35f, spring - .15f, z - .05f, .70f, .15f, .84f);
        }
    }

    const int stones = detailed ? 24 : 8;
    if (detailed) {
        // Recessed continuous masonry closes the joints without filling their relief.
        ArchRing(mesh, x, z + .045f, spring, inner + .024f, outer - .024f,
                 depth - .09f, stones, 0.f, false);
    }
    ArchRing(mesh, x, z, spring, inner, outer, depth, stones, detailed ? .005f : 0.f, detailed);
}

MeshData Harbor(bool detailed) {
    MeshData data;
    StoneBox(data, -3.05f, -1.05f, -10.f, 6.10f, 1.04f, 26.f);
    StoneBox(data, 3.10f, -1.05f, -5.f, 9.f, 1.04f, 14.f);
    StoneBox(data, 3.04f, -.06f, -4.5f, 1.30f, .06f, 13.f);
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
    StoneBox(data, 7.05f, 0.f, 3.50f, 4.73f, .18f, 4.80f);
    for (const float z : {-1.7f, 1.2f, 4.2f}) {
        StoneBox(data, 3.72f, 0.f, z, .28f, 1.11f, .30f);
        StoneBox(data, 3.45f, 1.11f, z - .22f, .82f, .09f, .74f);
        if (detailed) {
            StoneBox(data, 3.57f, .04f, z - .15f, .58f, .14f, .60f);
            StoneBox(data, 3.58f, 1.02f, z - .11f, .56f, .09f, .52f);
        }
    }
    if (detailed) {
        for (int i = 0; i < 8; ++i) {
            StoneBox(data, 1.82f, i * .43f, 12.94f, 1.26f, .025f, 2.9f);
        }
        for (int row = 0; row < 18; ++row) {
            for (int column = 0; column < 11; ++column) {
                StoneBox(data, 3.17f + column * .805f, -.015f, -4.91f + row * .77f,
                         .766f, .024f, .729f);
            }
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
            const float grain = .69f + .19f * CloudNoise(u * 6.f, v * 5.f) +
                .09f * CloudNoise(u * 17.f, v * 25.f) + .03f * Pattern(u * 4.f, v * 8.f);
            const auto at = (y * edge + x) * 4;
            pixels[at] = static_cast<uint8_t>(255.f * grain);
            pixels[at + 1] = static_cast<uint8_t>(231.f * grain);
            pixels[at + 2] = static_cast<uint8_t>(185.f * grain);
            pixels[at + 3] = 255;
        }
    }
    return renderer.CreateTexture(edge, edge, pixels.data());
}

Texture StoneNormals(Renderer& renderer) {
    constexpr unsigned edge = 128;
    std::vector<uint8_t> pixels(edge * edge * 4);
    for (unsigned y = 0; y < edge; ++y) {
        for (unsigned x = 0; x < edge; ++x) {
            const float u = static_cast<float>(x) * 2.f * kPi / edge;
            const float v = static_cast<float>(y) * 2.f * kPi / edge;
            const float dx = .09f * std::cos(2.f * u + 3.f * v) +
                .055f * std::sin(5.f * u - 6.f * v + 1.4f * std::sin(3.f * v));
            const float dy = .09f * std::cos(3.f * u + 2.f * v) +
                .050f * std::sin(7.f * u - 4.f * v + 1.3f * std::sin(2.f * u));
            const auto at = (y * edge + x) * 4;
            pixels[at] = static_cast<uint8_t>(127.5f * (1.f + dx));
            pixels[at + 1] = static_cast<uint8_t>(127.5f * (1.f + dy));
            pixels[at + 2] = static_cast<uint8_t>(127.5f * (1.f + std::sqrt(1.f - dx * dx - dy * dy)));
            pixels[at + 3] = 255;
        }
    }
    return renderer.CreateTexture(edge, edge, pixels.data());
}

Mesh SeaGrid(Renderer& renderer) {
    std::vector<float> columns;
    std::vector<float> rows;
    for (float x = -148.f; x < 148.f; x += std::abs(x) > 34.f ? 3.f : .30f) columns.push_back(x);
    columns.push_back(148.f);
    for (float z = -58.f; z < 135.f; z += z < -18.f || z > 44.f ? 2.2f : .30f) rows.push_back(z);
    rows.push_back(135.f);
    std::vector<PbrVertex> vertices;
    std::vector<uint32_t> indices;
    const auto stride = static_cast<uint32_t>(columns.size());
    for (std::size_t row = 0; row < rows.size(); ++row) {
        for (std::size_t column = 0; column < columns.size(); ++column) {
            vertices.push_back({{columns[column], -.64f, rows[row]}, {0, 1, 0}, {1, 0, 0, -1}, {0, 0}});
            if (row + 1 < rows.size() && column + 1 < columns.size()) {
                const auto index = static_cast<uint32_t>(row * stride + column);
                indices.insert(indices.end(), {index, index + 1, index + stride, index + 1,
                                               index + stride + 1, index + stride});
            }
        }
    }
    return renderer.CreatePbrMesh(vertices, indices);
}

struct ColoredMesh {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    void Face(std::initializer_list<XMFLOAT3> positions, XMFLOAT4 color) {
        const auto point = positions.begin();
        const auto origin = XMLoadFloat3(point);
        XMFLOAT3 normal;
        XMStoreFloat3(&normal, XMVector3Normalize(XMVector3Cross(XMLoadFloat3(point + 2) - origin,
                                                               XMLoadFloat3(point + 1) - origin)));
        const auto base = static_cast<uint32_t>(vertices.size());
        for (const auto& position : positions) vertices.push_back({position, normal, color});
        for (uint32_t corner = 1; corner + 1 < positions.size(); ++corner) {
            indices.insert(indices.end(), {base, base + corner, base + corner + 1});
        }
    }

    void Box(float x, float y, float z, float width, float height, float length, XMFLOAT4 color) {
        const float right = x + width, top = y + height, back = z + length;
        Face({{x, y, z}, {right, y, z}, {right, top, z}, {x, top, z}}, color);
        Face({{right, y, back}, {x, y, back}, {x, top, back}, {right, top, back}}, color);
        Face({{x, y, back}, {x, y, z}, {x, top, z}, {x, top, back}}, color);
        Face({{right, y, z}, {right, y, back}, {right, top, back}, {right, top, z}}, color);
        Face({{x, top, z}, {right, top, z}, {right, top, back}, {x, top, back}}, color);
    }

    Mesh Upload(Renderer& renderer) const { return renderer.CreateMesh(vertices, indices); }
};

Mesh Resort(Renderer& renderer) {
    ColoredMesh data;
    const XMFLOAT4 plaster{.76f, .55f, .34f, 1.f};
    const XMFLOAT4 tiles{.48f, .18f, .11f, 1.f};
    const XMFLOAT4 teak{.31f, .16f, .095f, 1.f};
    const XMFLOAT4 glass{.13f, .27f, .34f, 1.f};
    const XMFLOAT4 lamplight{1.f, .62f, .28f, 1.f};
    data.Box(7.42f, .18f, 4.02f, 4.19f, 2.65f, 4.17f, plaster);
    data.Box(7.23f, 2.71f, 3.80f, 4.58f, .12f, 4.60f, teak);
    data.Face({{7.18f, 2.82f, 3.73f}, {9.50f, 3.83f, 3.73f},
               {9.50f, 3.83f, 8.46f}, {7.18f, 2.82f, 8.46f}}, tiles);
    data.Face({{9.50f, 3.83f, 3.73f}, {11.87f, 2.82f, 3.73f},
               {11.87f, 2.82f, 8.46f}, {9.50f, 3.83f, 8.46f}}, tiles);
    data.Face({{11.87f, 2.82f, 3.74f}, {9.50f, 3.83f, 3.74f}, {7.18f, 2.82f, 3.74f}}, plaster);
    data.Face({{7.18f, 2.82f, 8.45f}, {9.50f, 3.83f, 8.45f}, {11.87f, 2.82f, 8.45f}}, plaster);
    for (int room = 0; room < 3; ++room) {
        const float z = 4.34f + room * 1.18f;
        data.Box(7.38f, .39f, z, .095f, 1.84f, .90f, teak);
        data.Box(7.35f, .47f, z + .065f, .04f, 1.68f, .77f, glass);
        data.Box(7.32f, 1.23f, z + .07f, .04f, .054f, .76f, teak);
        data.Box(7.31f, .48f, z + .44f, .04f, 1.66f, .032f, teak);
        data.Box(7.10f, 2.02f, z + .93f, .23f, .21f, .17f, lamplight);
    }
    data.Box(7.05f, 2.31f, 4.06f, .29f, .11f, 4.13f, teak);
    for (const float z : {-2.4f, .90f, 4.12f}) {
        constexpr float center = 5.25f;
        data.Box(center - .034f, 0.f, z - .034f, .068f, 2.29f, .068f, teak);
        for (int panel = 0; panel < 12; ++panel) {
            const float start = 2.f * kPi * panel / 12.f;
            const float end = 2.f * kPi * (panel + 1) / 12.f;
            const XMFLOAT4 canvas = panel % 2 == 0 ? XMFLOAT4{.86f, .79f, .62f, 1.f}
                : XMFLOAT4{.12f, .40f, .44f, 1.f};
            data.Face({{center, 2.38f, z}, {center + 1.12f * std::cos(start), 1.88f, z + 1.12f * std::sin(start)},
                       {center + 1.12f * std::cos(end), 1.88f, z + 1.12f * std::sin(end)}}, canvas);
        }
        for (const float offset : {-.72f, .30f}) {
            const float x = center + offset;
            data.Box(x, .10f, z - .92f, .57f, .10f, 1.62f, teak);
            data.Box(x + .045f, .20f, z - .82f, .48f, .09f, 1.27f, plaster);
            data.Face({{x + .04f, .29f, z + .07f}, {x + .53f, .29f, z + .07f},
                       {x + .53f, .64f, z + .59f}, {x + .04f, .64f, z + .59f}}, plaster);
        }
    }
    const XMFLOAT4 terracotta{.60f, .25f, .13f, 1.f};
    const XMFLOAT4 trunk{.32f, .25f, .16f, 1.f};
    const XMFLOAT4 leaf{.19f, .35f, .24f, 1.f};
    for (const XMFLOAT2 base : {XMFLOAT2{10.85f, -2.75f}, XMFLOAT2{11.08f, .81f}, XMFLOAT2{6.59f, 7.90f}}) {
        data.Box(base.x - .38f, 0.f, base.y - .38f, .76f, .36f, .76f, terracotta);
        data.Box(base.x - .09f, .34f, base.y - .09f, .18f, 2.42f, .18f, trunk);
        for (int branch = 0; branch < 9; ++branch) {
            const float angle = 2.f * kPi * branch / 9.f + base.y;
            const float side = std::cos(angle), forward = std::sin(angle);
            const XMFLOAT3 origin{base.x, 2.70f, base.y};
            const XMFLOAT3 mid{base.x + .69f * side, 3.25f, base.y + .69f * forward};
            const XMFLOAT3 tip{base.x + 1.56f * side, 2.59f, base.y + 1.56f * forward};
            const XMFLOAT3 right{mid.x + .15f * forward, mid.y - .07f, mid.z - .15f * side};
            const XMFLOAT3 left{mid.x - .15f * forward, mid.y - .07f, mid.z + .15f * side};
            data.Face({origin, right, mid}, leaf);
            data.Face({mid, right, tip}, leaf);
            data.Face({origin, mid, left}, leaf);
            data.Face({left, mid, tip}, leaf);
        }
    }
    return data.Upload(renderer);
}

Mesh Boat(Renderer& renderer) {
    ColoredMesh data;
    const XMFLOAT4 wood{.27f, .13f, .10f, 1.f};
    const XMFLOAT4 sail{.87f, .78f, .61f, 1.f};
    data.Box(-.48f, -.14f, -1.10f, .96f, .37f, 2.f, wood);
    data.Face({{-.48f, .23f, .90f}, { .48f, .23f, .90f}, {0.f, .23f, 1.61f}}, wood);
    data.Face({{.48f, .23f, .90f}, {0.f, -.10f, 1.42f}, {0.f, .23f, 1.61f}}, wood);
    data.Face({{0.f, .23f, 1.61f}, {0.f, -.10f, 1.42f}, {-.48f, .23f, .90f}}, wood);
    data.Box(-.04f, .20f, .24f, .08f, 3.45f, .08f, wood);
    data.Face({{0.f, 3.49f, .24f}, {-.13f, .55f, -1.06f}, {0.f, .55f, .19f}}, sail);
    return data.Upload(renderer);
}

Mesh Wing(Renderer& renderer, float side) {
    ColoredMesh data;
    const XMFLOAT4 feathers{.61f, .60f, .62f, 1.f};
    const XMFLOAT4 tips{.17f, .16f, .20f, 1.f};
    data.Face({{0.f, 0.f, -.115f}, {side * .43f, .035f, .065f},
               {side * .55f, .04f, .22f}, {0.f, 0.f, .18f}}, feathers);
    data.Face({{side * .43f, .035f, .065f}, {side * .88f, -.055f, -.10f},
               {side * .55f, .04f, .22f}}, tips);
    return data.Upload(renderer);
}
}

Scene::Scene(Renderer& renderer) {
    m_sky = Sky(renderer);
    m_islands = Islands(renderer);

    m_water = SeaGrid(renderer);
    m_waterMaterial = renderer.CreatePbrMaterial(nullptr, nullptr, nullptr);
    m_waterMaterial.waterParams.x = 1.f;
    m_resort = Resort(renderer);
    m_boat = Boat(renderer);
    m_leftWing = Wing(renderer, -1.f);
    m_rightWing = Wing(renderer, 1.f);

    const auto stoneTexture = StoneTexture(renderer);
    const auto stoneNormals = StoneNormals(renderer);
    for (const auto detail : {Detail::Simple, Detail::Detailed}) {
        const bool detailed = detail == Detail::Detailed;
        const auto data = Harbor(detailed);
        auto& buildings = detailed ? m_detailed : m_simple;
        buildings.geometry = renderer.CreatePbrMesh(data.vertices, data.indices);
        buildings.material = renderer.CreatePbrMaterial(detailed ? &stoneTexture : nullptr,
                                                        detailed ? &stoneNormals : nullptr, nullptr);
        buildings.material.baseColorFactor = {.69f, .52f, .34f, 1.f};
        buildings.material.normalScale = .72f;
        buildings.material.roughnessFactor = detailed ? .57f : .69f;
        buildings.material.metallicFactor = 0.f;
        buildings.triangles = data.indices.size() / 3;
    }
}

void Scene::Draw(Renderer& renderer, const Camera& camera, Detail detail, float time) {
    const auto identity = XMMatrixIdentity();
    renderer.SetFog(false);
    renderer.SetDirectionalLight({-kSun.x, -kSun.y, -kSun.z}, {1, .77f, .56f}, 1.34f,
                                 {.40f, .43f, .54f});
    renderer.SetShadowExtent(23.f);
    renderer.BeginFrame(camera);
    renderer.DrawMesh(m_sky, identity, true);
    renderer.DrawMesh(m_islands, identity);
    m_waterMaterial.waterParams.y = time;
    renderer.DrawPbrMesh(m_water, m_waterMaterial, identity);
    const auto& buildings = detail == Detail::Detailed ? m_detailed : m_simple;
    renderer.DrawPbrMesh(buildings.geometry, buildings.material, identity);
    renderer.DrawMesh(m_resort, identity);
    const float sailAngle = time * .13f;
    const auto boat = XMMatrixRotationZ(.12f * std::sin(1.7f * time)) * XMMatrixRotationY(sailAngle + .55f) *
        XMMatrixTranslation(-10.1f + 4.7f * std::sin(sailAngle), -.55f + .14f * std::sin(time * 1.47f),
                            7.1f + 3.8f * (1.f - std::cos(sailAngle)));
    renderer.DrawMesh(m_boat, boat);
    for (int bird = 0; bird < 4; ++bird) {
        const float phase = time * .39f + bird * .27f;
        const float flap = .50f * std::sin(time * (4.6f + bird * .25f) + bird * .91f);
        const auto flight = XMMatrixRotationY(phase - kPi * .5f) *
            XMMatrixTranslation(-5.f + 11.f * std::cos(phase), 4.9f + .52f * std::sin(phase * 2.f) + bird * .36f,
                                18.f - 9.f * std::sin(phase));
        renderer.DrawMesh(m_leftWing, XMMatrixRotationZ(flap) * flight);
        renderer.DrawMesh(m_rightWing, XMMatrixRotationZ(-flap) * flight);
    }
}

std::size_t Scene::TriangleCount(Detail detail) const {
    return detail == Detail::Detailed ? m_detailed.triangles : m_simple.triangles;
}
}
