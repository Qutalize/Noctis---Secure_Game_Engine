#include "game/sunset/SunsetScene.h"
#include "engine/renderer/PngLoader.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <iterator>
#include <numbers>
#include <stdexcept>
#include <vector>

using namespace DirectX;

namespace sunset {
namespace {
constexpr float kPi = std::numbers::pi_v<float>;

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

Mesh Sky(Renderer& renderer) {
    constexpr int rings = 20;
    constexpr int segments = 64;
    constexpr float radius = 380.f;
    std::vector<PbrVertex> vertices;
    std::vector<uint32_t> indices;
    for (int y = 0; y <= rings; ++y) {
            const float elevation = Mix(-kPi * .5f, kPi * .5f, static_cast<float>(y) / rings);
        for (int x = 0; x <= segments; ++x) {
            const float azimuth = 2.f * kPi * static_cast<float>(x) / segments;
            const XMFLOAT3 direction{std::sin(azimuth) * std::cos(elevation),
                                     std::sin(elevation), std::cos(azimuth) * std::cos(elevation)};
            vertices.push_back({{radius * direction.x, radius * direction.y, radius * direction.z},
                                {0, 1, 0}, {1, 0, 0, 1}, {0, 0}});
            if (x < segments && y < rings) {
                const auto index = static_cast<uint32_t>(y * (segments + 1) + x);
                const auto next = index + segments + 1;
                indices.insert(indices.end(), {index, next, index + 1, index + 1, next, next + 1});
            }
        }
    }
    return renderer.CreatePbrMesh(vertices, indices);
}

Mesh Islands(Renderer& renderer) {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    for (int layer = 0; layer < 2; ++layer) {
        constexpr int samples = 100;
        const auto start = static_cast<uint32_t>(vertices.size());
        const XMFLOAT4 color = layer == 0 ? XMFLOAT4{.44f, .47f, .54f, 1.f}
                                          : XMFLOAT4{.39f, .43f, .50f, 1.f};
        for (int i = 0; i <= samples; ++i) {
            const float x = Mix(-120.f, 120.f, static_cast<float>(i) / samples);
            const float envelope = std::exp(-std::pow((x + 47.f - layer * 10.f) / 29.f, 2.f)) +
                                   .5f * std::exp(-std::pow((x - 83.f) / 14.f, 2.f));
            const float top = envelope * (2.2f + 6.f * Pattern(x * .013f, layer * .3f)) - 1.4f;
            const float z = 235.f - layer * 24.f;
            vertices.push_back({{x, -2.f, z}, {0, 1, 0}, color});
            vertices.push_back({{x, top, z}, {0, 1, 0}, color});
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
    StoneBox(data, 3.10f, -.26f, -5.f, 9.f, .25f, 14.f);
    StoneBox(data, 3.04f, -.06f, -4.5f, 1.30f, .06f, 13.f);
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


Mesh SeaGrid(Renderer& renderer) {
    std::vector<float> columns;
    std::vector<float> rows;
    for (float x = -309.f; x < 309.f; x += std::abs(x) > 34.f ? 5.f : .30f) columns.push_back(x);
    columns.push_back(309.f);
    for (float z = -58.f; z < 309.f; z += z < -18.f || z > 44.f ? 4.f : .30f) rows.push_back(z);
    rows.push_back(309.f);
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

float Shoreline(float x) {
    return -11.f + 1.55f * std::sin(.17f * x) + .54f * std::sin(.39f * x + 1.4f);
}

void Palm(ColoredMesh& mesh, float x, float y, float z, float size, float seed) {
    constexpr int trunkSides = 7;
    constexpr int trunkSteps = 7;
    constexpr int frondCount = 10;
    const auto ring = [=](int step, int side) {
        const float height = static_cast<float>(step) / trunkSteps;
        const float angle = 2.f * kPi * side / trunkSides;
        const float radius = size * Mix(.145f, .067f, height);
        return XMFLOAT3{x + radius * std::cos(angle) + size * .49f * height * height * std::cos(seed),
                        y + size * height * 2.7f, z + radius * std::sin(angle) + size * .36f * height * height};
    };
    for (int step = 0; step < trunkSteps; ++step) {
        for (int side = 0; side < trunkSides; ++side) {
            const float tone = .31f + .075f * Pattern(step * .58f, side * .31f);
            mesh.Face({ring(step, side), ring(step + 1, side), ring(step + 1, side + 1), ring(step, side + 1)},
                      {tone, tone * .78f, tone * .52f, 1});
        }
    }
    const float centerX = x + size * .49f * std::cos(seed);
    const float centerZ = z + size * .36f;
    for (int branch = 0; branch < frondCount; ++branch) {
        const float angle = kPi * 2.f * branch / frondCount + seed;
        const float axisX = std::cos(angle), axisZ = std::sin(angle);
        const float length = size * (1.27f + .59f * Pattern(angle, seed));
        const float lift = size * (.74f + .39f * Pattern(seed, angle));
        const auto leaf = [&](float progress, float width) {
            return XMFLOAT3{centerX + length * progress * axisX + width * axisZ,
                            y + size * 2.63f + lift * std::sin(progress * kPi) - size * .54f * progress * progress,
                            centerZ + length * progress * axisZ - width * axisX};
        };
        for (int strip = 0; strip < 11; ++strip) {
            const float start = static_cast<float>(strip) / 11.f;
            const float end = (strip + 1.f) / 11.f;
            const float width = .29f * size * std::sin(kPi * (.06f + .88f * start));
            const float tone = .23f + .22f * Pattern(seed + angle, start * 3.f);
            for (const float side : {-1.f, 1.f}) {
                mesh.Face({leaf(start, 0.f), leaf(end, 0.f), leaf(std::min(1.f, end + .08f), width * side)},
                          {tone * .48f, tone, tone * .43f, 1.f});
            }
        }
    }
}

XMFLOAT3 BeachVertex(float x, float distance) {
    const float hills = std::clamp((distance - 3.5f) * .28f, 0.f, 1.f);
    const float side = std::clamp(std::abs(x) * .12f, 0.f, 1.f);
    const float height = -.64f + std::min(distance, 5.f) * .13f +
        hills * side * (CloudNoise(x * .17f, distance * .24f) + .6f) * 1.3f;
    return {x, height, Shoreline(x) - distance};
}

Mesh Shore(Renderer& renderer) {
    ColoredMesh mesh;
    for (int tree = 0; tree < 16; ++tree) {
        const float x = (tree < 8 ? -1.f : 1.f) * (4.55f + (tree % 8) * 2.06f);
        const float inland = 3.9f + 5.2f * CloudNoise(tree * .82f, 1.36f);
        const auto base = BeachVertex(x, inland);
        Palm(mesh, base.x, base.y - .13f, base.z,
             .72f + .45f * CloudNoise(tree * 1.33f, 3.1f), tree * 1.62f);
    }
    for (int rock = 0; rock < 21; ++rock) {
        const float seed = rock * 1.71f;
        const float x = -31.f + 25.f * CloudNoise(seed, 5.26f);
        const float inland = -.38f + CloudNoise(1.9f, seed) * 3.4f;
        const auto base = BeachVertex(x, inland);
        const float radius = .19f + 1.36f * std::pow(CloudNoise(seed, 6.4f), 1.7f);
        const auto ring = [&](int index, bool top) {
            const int side = index % 7;
            const float angle = kPi * 2.f * side / 7.f + seed;
            const float width = radius * (.64f + .51f * CloudNoise(seed, side * 1.7f));
            const float scale = top ? .56f : 1.f;
            return XMFLOAT3{base.x + scale * width * std::cos(angle) + (top ? .16f * radius : 0.f),
                            base.y + (top ? (.36f + .19f * CloudNoise(side, seed)) * radius : -.16f),
                            base.z + scale * width * .82f * std::sin(angle)};
        };
        for (int side = 0; side < 7; ++side) {
            const float shade = .34f + .18f * CloudNoise(seed, side);
            const XMFLOAT4 color{shade, shade * .95f, shade * .87f, 1.f};
            mesh.Face({ring(side, false), ring(side + 1, false), ring(side + 1, true), ring(side, true)}, color);
            mesh.Face({{base.x + radius * .15f, base.y + radius * .54f, base.z},
                       ring(side + 1, true), ring(side, true)}, color);
        }
    }
    return mesh.Upload(renderer);
}

Texture LoadCoastalMap(Renderer& renderer, const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary | std::ios::ate);
    const auto length = stream.tellg();
    if (length <= 0 || length > 8 * 1024 * 1024) throw std::runtime_error("Missing coastal material map");
    std::vector<uint8_t> bytes(static_cast<std::size_t>(length));
    stream.seekg(0);
    PngImage png;
    std::string error;
    if (!stream.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size())) ||
        !DecodePng(bytes.data(), bytes.size(), png, &error)) {
        throw std::runtime_error("Could not decode coastal material map: " + error);
    }
    return renderer.CreateTexture(png.width, png.height, png.rgba.data());
}

Mesh Foreshore(Renderer& renderer) {
    MeshData mesh;
    for (int strip = -55; strip < 55; ++strip) {
        const float left = static_cast<float>(strip);
        const float right = left + 1.f;
        for (int band = 0; band < 22; ++band) {
            const float seaward = -.78f + 1.35f * band;
            const float inland = seaward + 1.35f;
            mesh.Quad({{BeachVertex(left, inland), BeachVertex(right, inland),
                        BeachVertex(right, seaward), BeachVertex(left, seaward)}});
        }
    }
    for (auto& vertex : mesh.vertices) {
        vertex.uv = {vertex.position.x / 3.2f, vertex.position.z / 3.2f};
    }
    return renderer.CreatePbrMesh(mesh.vertices, mesh.indices);
}

Mesh RuinIsland(Renderer& renderer) {
    ColoredMesh mesh;
    constexpr XMFLOAT4 cliff{.39f, .41f, .39f, 1};
    constexpr XMFLOAT4 masonry{.43f, .46f, .39f, 1};
    const float islandX = -31.f, islandZ = 129.f;
    for (int face = 0; face < 19; ++face) {
        const auto coast = [&](int section, bool top) {
            const float angle = kPi * 2.f * (section % 19) / 19.f;
            const float rock = .75f + .25f * CloudNoise(section % 19 * 1.7f, 9.f);
            return XMFLOAT3{islandX + (top ? 17.f : 22.f) * rock * std::cos(angle),
                            top ? 3.f + 3.f * rock : -1.7f,
                            islandZ + (top ? 11.f : 17.f) * rock * std::sin(angle)};
        };
        mesh.Face({coast(face, false), coast(face + 1, false), coast(face + 1, true), coast(face, true)}, cliff);
        mesh.Face({{islandX, 6.5f, islandZ}, coast(face, true), coast(face + 1, true)}, masonry);
    }
    return mesh.Upload(renderer);
}

Mesh Restaurant(Renderer& renderer) {
    ColoredMesh mesh;
    constexpr XMFLOAT4 beam{.36f, .21f, .12f, 1};
    constexpr XMFLOAT4 canvas{.84f, .79f, .65f, 1};
    for (int board = 0; board < 41; ++board) {
        mesh.Box(-6.06f, -.18f, 18.6f + board * .27f, 12.11f, .22f, .258f, beam);
    }
    for (const float x : {-5.6f, 5.38f}) {
        const float eave = 3.13f + .025f * std::abs(x);
        const float ridge = 5.05f + .033f * x;
        for (const float z : {18.9f, 22.9f, 27.6f}) {
            const float roof = Mix(ridge, eave, std::abs(z - 24.4f) / (z < 24.4f ? 6.1f : 5.6f));
            mesh.Box(x, -2.1f, z, .24f, roof + 2.1f, .24f, beam);
        }
    }
    for (int panel = 0; panel < 24; ++panel) {
        const float x = -6.56f + panel * .55f;
        const float next = -6.56f + (panel + 1) * .55f;
        const float eave = 3.13f + .025f * std::abs(x);
        const float nextEave = 3.13f + .025f * std::abs(next);
        const float ridge = 5.05f + .033f * x;
        const float nextRidge = 5.05f + .033f * next;
        const float tier = panel % 2 ? 1.f : .91f;
        const XMFLOAT4 roof{canvas.x * tier, canvas.y * tier, canvas.z * tier, 1.f};
        mesh.Face({{x,eave,18.3f},{next,nextEave,18.3f},{next,nextRidge,24.4f},{x,ridge,24.4f}}, roof);
        mesh.Face({{x,ridge,24.4f},{next,nextRidge,24.4f},{next,nextEave,30.0f},{x,eave,30.0f}}, roof);
        mesh.Face({{x,ridge - .07f,24.4f},{next,nextRidge - .07f,24.4f},{next,nextEave - .08f,18.3f},{x,eave - .08f,18.3f}}, beam);
        mesh.Face({{x,eave - .08f,30.f},{next,nextEave - .08f,30.f},{next,nextRidge - .07f,24.4f},{x,ridge - .07f,24.4f}}, beam);
    }
    for (float z : {21.9f, 24.8f, 27.25f}) {
        mesh.Box(-5.73f, 3.52f, z - .095f, 11.46f, .23f, .19f, beam);
    }
    mesh.Box(-2.45f, .03f, 28.38f, 4.90f, .95f, .93f, beam);
    mesh.Box(-2.62f, .98f, 28.29f, 5.24f, .10f, 1.12f, canvas);
    return mesh.Upload(renderer);
}

Mesh Lanterns(Renderer& renderer) {
    MeshData mesh;
    for (const float side : {-1.f, 1.f}) {
        for (const float z : {21.9f, 24.8f, 27.25f}) {
            StoneBox(mesh, side * 3.46f - .055f, 2.999f, z - .055f, .11f, .12f, .11f);
        }
    }
    for (const float center : {4.74f, 6.09f, 7.44f}) {
        for (const float offset : {-.39f, .063f}) {
            const float bottom = center > 5.f && center < 7.f ? .51f : .89f;
            StoneBox(mesh, 7.473f, bottom, center + offset, .013f, 2.1f - bottom, .31f);
        }
    }
    return renderer.CreatePbrMesh(mesh.vertices, mesh.indices);
}

Mesh Resort(Renderer& renderer) {
    ColoredMesh data;
    const XMFLOAT4 plaster{.76f, .55f, .34f, 1.f};
    const XMFLOAT4 teak{.31f, .16f, .095f, 1.f};
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
    for (const XMFLOAT2 base : {XMFLOAT2{10.85f, -2.75f}, XMFLOAT2{11.08f, .81f}, XMFLOAT2{6.59f, 7.90f}}) {
        data.Box(base.x - .38f, 0.f, base.y - .38f, .76f, .36f, .76f, terracotta);
        Palm(data, base.x, .34f, base.y, 1.02f, base.y);
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
    wchar_t executable[32768]{};
    const auto pathSize = GetModuleFileNameW(nullptr, executable, static_cast<DWORD>(std::size(executable)));
    if (!pathSize || pathSize >= std::size(executable)) throw std::runtime_error("Cannot locate coastal assets");
    const auto models = std::filesystem::path(executable).parent_path() / "coastal-assets";
    const auto load = [&](GltfProp& prop, const char* name) {
        const auto path = (models / name / (std::string(name) + "_1k.gltf")).u8string();
        if (!prop.Load(renderer, std::string(path.begin(), path.end()), true)) {
            throw std::runtime_error(std::string("Missing coastal asset: ") + name);
        }
    };
    load(m_rock, "rock_face_01");
    load(m_diningSet, "outdoor_table_chair_set_01");
    load(m_chandelier, "lantern_chandelier_01");
    load(m_boardwalk, "pier");
    load(m_villa, "villa");
    load(m_ruins, "ruins");
    m_sky = Sky(renderer);
    m_skyMaterial = renderer.CreatePbrMaterial(nullptr, nullptr, nullptr);
    m_skyMaterial.coastalParams.x = 2.f;
    m_islands = Islands(renderer);
    m_ruinIsland = RuinIsland(renderer);
    m_shore = Shore(renderer);
    m_foreshore = Foreshore(renderer);
    const auto sand = models / "coast_sand_01";
    const auto sandColor = LoadCoastalMap(renderer, sand / "diff.png");
    const auto sandNormal = LoadCoastalMap(renderer, sand / "nor_gl.png");
    const auto sandOrm = LoadCoastalMap(renderer, sand / "arm.png");
    m_sandMaterial = renderer.CreatePbrMaterial(&sandColor, &sandNormal, &sandOrm);
    m_sandMaterial.baseColorSrgb = true;
    m_sandMaterial.coastalParams.x = -1.f;
    m_sandMaterial.baseColorFactor = {1.f, .97f, .89f, 1.f};
    m_sandMaterial.normalScale = .58f;
    m_sandMaterial.metallicFactor = 0.f;
    m_restaurant = Restaurant(renderer);
    m_lanterns = Lanterns(renderer);
    m_lanternMaterial = renderer.CreatePbrMaterial(nullptr, nullptr, nullptr);
    m_lanternMaterial.baseColorFactor = {1.f, .65f, .25f, 1.f};
    m_lanternMaterial.coastalParams.x = 3.f;

    m_water = SeaGrid(renderer);
    m_waterMaterial = renderer.CreatePbrMaterial(&sandColor, &sandNormal, &sandOrm);
    m_waterMaterial.coastalParams.x = 1.f;
    m_resort = Resort(renderer);
    m_boat = Boat(renderer);
    m_leftWing = Wing(renderer, -1.f);
    m_rightWing = Wing(renderer, 1.f);

    const auto stone = models / "large_sandstone_blocks_01";
    const auto stoneTexture = LoadCoastalMap(renderer, stone / "diff.png");
    const auto stoneNormals = LoadCoastalMap(renderer, stone / "nor_gl.png");
    const auto stoneOrm = LoadCoastalMap(renderer, stone / "arm.png");
    for (const auto detail : {Detail::Simple, Detail::Detailed}) {
        const bool detailed = detail == Detail::Detailed;
        const auto data = Harbor(detailed);
        auto& buildings = detailed ? m_detailed : m_simple;
        buildings.geometry = renderer.CreatePbrMesh(data.vertices, data.indices);
        buildings.material = renderer.CreatePbrMaterial(detailed ? &stoneTexture : nullptr,
                                                        detailed ? &stoneNormals : nullptr,
                                                        detailed ? &stoneOrm : nullptr);
        buildings.material.baseColorFactor = detailed ? XMFLOAT4{1.f, .97f, .89f, 1.f}
                                                       : XMFLOAT4{.69f, .52f, .34f, 1.f};
        buildings.material.baseColorSrgb = detailed;
        buildings.material.normalScale = .72f;
        buildings.material.roughnessFactor = detailed ? .57f : .69f;
        buildings.material.metallicFactor = 0.f;
        buildings.triangles = data.indices.size() / 3;
    }
}

void Scene::Draw(Renderer& renderer, const Camera& camera, Detail detail, float time, float daylight) {
    const auto identity = XMMatrixIdentity();
    const float sunset = std::clamp(daylight * 1.68f, 0.f, 1.f);
    const float twilight = std::clamp((daylight - .61f) / .39f, 0.f, 1.f);
    const float dusk = twilight * twilight * (3.f - 2.f * twilight);
    const float elevation = Mix(.72f, -.065f, daylight);
    const float azimuth = Mix(-.91f, -.46f, daylight);
    const XMFLOAT3 sun{std::sin(azimuth) * std::cos(elevation), std::sin(elevation),
                       std::cos(azimuth) * std::cos(elevation)};
    renderer.SetFog(false);
    renderer.SetCoastalHaze({Mix(.54f, .29f, dusk), Mix(.67f, .34f, dusk), Mix(.74f, .46f, dusk)},
                            Mix(.00275f, .0011f, dusk));
    renderer.SetDirectionalLight({-sun.x, -sun.y, -sun.z},
                                 {1.f, Mix(.93f, .64f, sunset), Mix(.86f, .36f, sunset)},
                                 Mix(1.07f, .03f, dusk),
                                 {Mix(.64f, .129f, dusk), Mix(.69f, .17f, dusk), Mix(.75f, .28f, dusk)});
    renderer.SetWarmFillLight({-3.46f, 2.98f, 24.8f}, 1.35f * dusk);
    renderer.SetWarmAccentLights({3.46f, 2.98f, 24.8f, 1.44f * dusk},
                                 {.3f, 2.78f, 27.0f, 1.19f * dusk});
    renderer.SetShadowExtent(42.f);
    renderer.BeginFrame(camera);
    m_skyMaterial.coastalParams = {2.f, time, sunset, dusk};
    const auto& eye = camera.GetPosition();
    renderer.DrawPbrMesh(m_sky, m_skyMaterial, XMMatrixTranslation(eye.x, eye.y, eye.z), true);
    renderer.DrawMesh(m_islands, identity);
    renderer.DrawMesh(m_ruinIsland, identity);
    m_ruins.Draw(renderer, identity);
    for (const auto& [x, y, z, scale, rotation] : {
             std::array{-41.f, -2.3f, 124.f, 3.25f, .32f},
             std::array{-29.6f, -2.9f, 119.7f, 3.55f, -.12f},
             std::array{-18.2f, -2.95f, 123.f, 3.15f, -.46f},
             std::array{-49.7f, -2.25f, 129.2f, 3.0f, .86f}}) {
        m_rock.Draw(renderer, XMMatrixScaling(scale, scale, scale) * XMMatrixRotationY(rotation) *
                             XMMatrixTranslation(x, y, z));
    }
    renderer.DrawMesh(m_shore, identity);
    renderer.DrawPbrMesh(m_foreshore, m_sandMaterial, identity);
    m_waterMaterial.coastalParams = {1.f, time, sunset, dusk};
    renderer.DrawPbrMesh(m_water, m_waterMaterial, identity);
    m_boardwalk.Draw(renderer, identity);
    renderer.DrawMesh(m_restaurant, identity);
    for (const float side : {-1.f, 1.f}) {
        for (const float z : {21.9f, 24.8f, 27.25f}) {
            m_diningSet.Draw(renderer, XMMatrixScaling(1.42f, 1.42f, 1.42f) *
                                      XMMatrixRotationY(side * .72f) *
                                      XMMatrixTranslation(side * 3.46f, .055f, z));
            m_chandelier.Draw(renderer, XMMatrixScaling(1.01f, 1.01f, 1.01f) *
                                        XMMatrixTranslation(side * 3.46f, 3.56f, z));
        }
    }
    m_lanternMaterial.coastalParams.w = dusk;
    renderer.DrawPbrMesh(m_lanterns, m_lanternMaterial, identity);
    const auto& buildings = detail == Detail::Detailed ? m_detailed : m_simple;
    renderer.DrawPbrMesh(buildings.geometry, buildings.material, identity);
    renderer.DrawMesh(m_resort, identity);
    m_villa.Draw(renderer, identity);
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
