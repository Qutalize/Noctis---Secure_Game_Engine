#pragma once

#include "game/GltfProp.h"

#include <filesystem>
#include <stdexcept>

// These background figures use the artist's baked poses. The placement uses metres.
class ScenicPeople {
public:
    void Load(Renderer& renderer, const std::filesystem::path& assets, const char* name) {
        const auto file = (assets / "people" / (std::string(name) + ".gltf")).u8string();
        if (!m_figure.Load(renderer, std::string(file.begin(), file.end()), true, true)) {
            throw std::runtime_error(std::string("Missing scenic figure: ") + name);
        }
        m_figure.SetScenicEffect({-6.f, 0.f, 0.f, 0.f});
    }

    void Draw(Renderer& renderer, const Camera& camera, const DirectX::XMFLOAT3& feet, float yaw) const {
        constexpr float visibility = 118.f;
        const auto& eye = camera.GetPosition();
        const float dx = feet.x - eye.x, dz = feet.z - eye.z;
        if (dx * dx + dz * dz > visibility * visibility) return;
        const auto world = DirectX::XMMatrixTranslation(0.f, -m_figure.BoundsMin().y, 0.f) *
                           DirectX::XMMatrixRotationY(yaw) *
                           DirectX::XMMatrixTranslation(feet.x, feet.y, feet.z);
        m_figure.Draw(renderer, world);
    }

private:
    GltfProp m_figure;
};
