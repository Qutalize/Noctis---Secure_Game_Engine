#include "game/sunset/CoastalAudio.h"

#include <windows.h>
#include <wrl/client.h>
#include <xaudio2.h>

#include <array>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <vector>

namespace sunset {
namespace {
constexpr DirectX::XMFLOAT3 kVilla{7.35f, 1.3f, 5.85f};
constexpr DWORD kRate = 48000;
constexpr WORD kChannels = 2;
constexpr WORD kBits = 16;

struct Voice {
    IXAudio2SourceVoice* source = nullptr;
    std::vector<BYTE> samples;

    ~Voice() {
        Reset();
    }

    void Reset() {
        if (source) source->DestroyVoice();
        source = nullptr;
    }

    void Start(IXAudio2& engine, const std::filesystem::path& file, float level) {
        std::ifstream stream(file, std::ios::binary | std::ios::ate);
        const auto size = stream.tellg();
        std::array<BYTE, 44> header{};
        if (size <= static_cast<std::streamoff>(header.size()) || size > 8 * 1024 * 1024) return;
        stream.seekg(0);
        if (!stream.read(reinterpret_cast<char*>(header.data()), header.size())) return;
        const auto read32 = [&header](std::size_t index) {
            DWORD value;
            std::memcpy(&value, header.data() + index, sizeof(value));
            return value;
        };
        const auto bytes = static_cast<DWORD>(size - static_cast<std::streamoff>(header.size()));
        const WAVEFORMATEX format{WAVE_FORMAT_PCM, kChannels, kRate, kRate * 4, 4, kBits, 0};
        if (std::memcmp(header.data(), "RIFF", 4) || std::memcmp(header.data() + 8, "WAVEfmt ", 8) ||
            std::memcmp(header.data() + 36, "data", 4) || read32(16) != 16 ||
            std::memcmp(header.data() + 20, &format, 16) || read32(4) != bytes + 36 ||
            read32(40) != bytes || bytes % format.nBlockAlign) return;
        samples.resize(bytes);
        if (!stream.read(reinterpret_cast<char*>(samples.data()), bytes)) return;
        if (FAILED(engine.CreateSourceVoice(&source, &format))) return;
        XAUDIO2_BUFFER buffer{};
        buffer.AudioBytes = bytes;
        buffer.pAudioData = samples.data();
        buffer.LoopCount = XAUDIO2_LOOP_INFINITE;
        if (FAILED(source->SetVolume(level)) || FAILED(source->SubmitSourceBuffer(&buffer)) ||
            FAILED(source->Start())) {
            Reset();
        }
    }
};
}

struct CoastalAudio::State {
    Microsoft::WRL::ComPtr<IXAudio2> engine;
    IXAudio2MasteringVoice* master = nullptr;
    Voice sea;
    Voice music;
    bool com = false;

    ~State() {
        if (engine) engine->StopEngine();
        music.Reset();
        sea.Reset();
        if (master) master->DestroyVoice();
        engine.Reset();
        if (com) CoUninitialize();
    }
};

float VillaGain(const DirectX::XMFLOAT3& listener) {
    const float distance = std::hypot(listener.x - kVilla.x, listener.z - kVilla.z);
    const float relative = distance / 6.3f;
    return 1.f / (1.f + relative * relative * relative);
}

CoastalAudio::CoastalAudio() : m_state(std::make_unique<State>()) {
    wchar_t executable[32768];
    const DWORD length = GetModuleFileNameW(nullptr, executable, static_cast<DWORD>(std::size(executable)));
    if (length == 0 || length >= std::size(executable)) return;
    m_state->com = SUCCEEDED(CoInitializeEx(nullptr, COINIT_MULTITHREADED));
    if (FAILED(XAudio2Create(m_state->engine.GetAddressOf())) ||
        FAILED(m_state->engine->CreateMasteringVoice(&m_state->master))) return;
    const auto directory = std::filesystem::path(executable).parent_path();
    m_state->sea.Start(*m_state->engine.Get(), directory / L"sunset-ambience.wav", .82f);
    m_state->music.Start(*m_state->engine.Get(), directory / L"villa-music.wav", 0.f);
}

CoastalAudio::~CoastalAudio() = default;

void CoastalAudio::Update(const DirectX::XMFLOAT3& listener) {
    if (m_state->music.source) m_state->music.source->SetVolume(VillaGain(listener));
}
}
