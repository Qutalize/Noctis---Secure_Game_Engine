#pragma once

#include <DirectXMath.h>
#include <memory>

namespace sunset {
float VillaGain(const DirectX::XMFLOAT3& listener);

class CoastalAudio {
public:
    CoastalAudio();
    ~CoastalAudio();
    void Update(const DirectX::XMFLOAT3& listener);

private:
    struct State;
    std::unique_ptr<State> m_state;
};
}
