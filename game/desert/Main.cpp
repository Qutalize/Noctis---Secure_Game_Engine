#include "game/desert/DesertScene.h"
#include "engine/core/ConsoleUtf8.h"
#include "engine/core/Window.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string_view>

using namespace DirectX;
namespace fs=std::filesystem;

namespace {
constexpr int kWidth=960, kHeight=540, kFrameRate=24, kSeconds=12;
constexpr float kEyeHeight=1.63f;

fs::path AssetDirectory() {
    wchar_t executable[32768]{};
    const auto length=GetModuleFileNameW(nullptr,executable,static_cast<DWORD>(std::size(executable)));
    if (!length || length>=std::size(executable)) throw std::runtime_error("Cannot locate DesertRoad");
    return fs::path(executable).parent_path()/"desert-assets";
}

Camera LookAt(const XMFLOAT3& eye,const XMFLOAT3& target) {
    Camera camera;
    camera.SetPosition(eye);
    const float right=target.x-eye.x, forward=target.z-eye.z;
    camera.SetRotation(std::atan2(right,forward),std::atan2(target.y-eye.y,std::hypot(right,forward)));
    return camera;
}

struct Shot {
    Camera camera;
    float verticalFov;
    float evening;
};

Shot Compose(const desert::Route& route,float time) {
    if (time<4.f) {
        const float movement=time/4.f;
        return {LookAt(route.Point(4.f+movement*11.f,-15.f+movement*3.f,9.f-movement*2.f),
                       route.Point(101.f,-3.4f,3.2f)),42.f,.22f};
    }
    if (time<8.f) {
        const float movement=(time-4.f)/4.f;
        const float distance=15.f+movement*41.f;
        return {LookAt(route.Point(distance,2.05f+.34f*std::sin(movement*3.f),1.18f),
                       route.Point(distance+51.f,-.52f,2.35f)),48.f,.47f};
    }
    const float movement=(time-8.f)/4.f;
    return {LookAt(route.Point(109.f+movement*14.f,12.f-movement*5.7f,3.6f+movement*.84f),
                   route.Point(178.f+movement*10.f,-2.6f,4.95f)),39.f,.77f};
}

void SelfTest(const desert::Route& route) {
    auto start=route.At(30.f);
    if (std::abs(start.x-2.f)>.001f || std::abs(start.y-.33f)>.001f) {
        throw std::runtime_error("Road key differs from authored route");
    }
    auto middle=route.At(47.5f);
    if (std::abs(middle.x-1.55f)>.001f || std::abs(middle.y-.565f)>.001f) {
        throw std::runtime_error("Road interpolation failed");
    }
    auto end=route.Step({0.f,300.f},{50.f,120.f});
    if (end.y!=375.f || std::abs(end.x-route.At(end.y).x-4.62f)>.001f) {
        throw std::runtime_error("Walking corridor boundary failed");
    }
    for (int frame=0; frame<kSeconds*kFrameRate; ++frame) {
        const auto shot=Compose(route,static_cast<float>(frame)/kFrameRate);
        if (!std::isfinite(shot.camera.GetYaw()) || shot.camera.GetPosition().y < 1.f) {
            throw std::runtime_error("Invalid desert camera");
        }
    }
    std::puts("Desert checks passed: road keys, interpolation, walking boundary, 288 camera samples");
}

void SavePpm(const fs::path& path,const std::vector<uint8_t>& rgba) {
    if (rgba.size()!=static_cast<std::size_t>(kWidth*kHeight*4)) throw std::runtime_error("Invalid desert image");
    std::vector<uint8_t> rgb;
    rgb.reserve(kWidth*kHeight*3);
    for (std::size_t offset=0; offset<rgba.size(); offset+=4) {
        rgb.insert(rgb.end(),{rgba[offset],rgba[offset+1],rgba[offset+2]});
    }
    std::ofstream output(path,std::ios::binary);
    output<<"P6\n"<<kWidth<<' '<<kHeight<<"\n255\n";
    output.write(reinterpret_cast<const char*>(rgb.data()),static_cast<std::streamsize>(rgb.size()));
    if (!output) throw std::runtime_error("Could not write desert image");
}

void RequirePbr(const Renderer& renderer) {
    if (!renderer.IsPbrAvailable() || !renderer.IsShadowEnabled()) throw std::runtime_error("Missing PBR or shadows");
}

int Capture(const fs::path& directory) {
    if (!fs::create_directory(directory)) throw std::runtime_error("Desert render directory must be new");
    Renderer renderer;
    renderer.Initialize(nullptr,kWidth,kHeight,false,true);
    RequirePbr(renderer);
    if (renderer.SampleCount()!=4) throw std::runtime_error("WARP did not provide 4x MSAA");
    desert::Scene scene(renderer,AssetDirectory());
    for (int frame=0; frame<kSeconds*kFrameRate; ++frame) {
        const float time=static_cast<float>(frame)/kFrameRate;
        const auto shot=Compose(scene.GetRoute(),time);
        scene.Draw(renderer,shot.camera,time,shot.evening,shot.verticalFov);
        renderer.EndFrame();
        char filename[24]{};
        std::snprintf(filename,sizeof(filename),"frame-%04d.ppm",frame);
        SavePpm(directory/filename,renderer.ReadOffscreenRgba());
    }
    renderer.Shutdown();
    return 0;
}

int Play() {
    Window window;
    if (!window.Initialize(GetModuleHandleW(nullptr),kWidth,kHeight,L"Noctis | Desert Road")) {
        throw std::runtime_error("Window initialization failed");
    }
    Renderer renderer;
    renderer.Initialize(window.GetHandle(),kWidth,kHeight,false,true);
    RequirePbr(renderer);
    desert::Scene scene(renderer,AssetDirectory());
    const auto& route=scene.GetRoute();
    const wchar_t* title=renderer.SampleCount()==4
        ? L"Noctis | Desert Road | 4x MSAA | WASD / Shift / Tab / R"
        : L"Noctis | Desert Road | AA off | WASD / Shift / Tab / R";
    SetWindowTextW(window.GetHandle(),title);
    XMFLOAT2 position{1.91f,2.f};
    float yaw=-.035f, pitch=.025f, time=0.f;
    bool tabHeld=false;
    auto previous=std::chrono::steady_clock::now();
    auto& input=window.GetInput();
    while (window.ProcessMessages()) {
        input.BeginFrame(window.GetHandle());
        if (input.IsKeyDown(VK_ESCAPE)) break;
        const auto now=std::chrono::steady_clock::now();
        const float seconds=std::min(.05f,std::chrono::duration<float>(now-previous).count());
        previous=now;
        time+=seconds;
        const bool tab=input.IsKeyDown(VK_TAB);
        if (tab && !tabHeld) input.SetCursorLock(!input.IsCursorLocked(),window.GetHandle());
        tabHeld=tab;
        if (input.IsKeyDown('R')) {
            position={1.91f,2.f};
            yaw=-.035f;
            pitch=.025f;
        }
        if (input.IsCursorLocked()) {
            int dx=0,dy=0;
            input.GetMouseDelta(dx,dy);
            yaw=std::remainder(yaw+dx*.0023f,6.2831853f);
            pitch=std::clamp(pitch-dy*.0023f,-1.3f,1.3f);
        }
        const float strafe=static_cast<float>(input.IsKeyDown('D')-input.IsKeyDown('A'));
        const float forward=static_cast<float>(input.IsKeyDown('W')-input.IsKeyDown('S'));
        const float speed=input.IsKeyDown(VK_SHIFT)?9.f:3.5f;
        const float step=speed*seconds/std::max(1.f,std::hypot(strafe,forward));
        position=route.Step(position,{step*(strafe*std::cos(yaw)+forward*std::sin(yaw)),
                                      step*(forward*std::cos(yaw)-strafe*std::sin(yaw))});
        Camera camera;
        camera.SetPosition({position.x,route.At(position.y).y+kEyeHeight,position.y});
        camera.SetRotation(yaw,pitch);
        scene.Draw(renderer,camera,time,.47f,57.f);
        renderer.EndFrame();
    }
    input.SetCursorLock(false,window.GetHandle());
    renderer.Shutdown();
    return 0;
}
}

int wmain(int argc,wchar_t** argv) {
    EnableConsoleUtf8();
    try {
        if (argc==1) return Play();
        const std::wstring_view command=argv[1];
        if (argc==2 && command==L"--self-test") {
            SelfTest(desert::Route(AssetDirectory()/"route.json"));
            return 0;
        }
        if (argc==3 && command==L"--capture") return Capture(argv[2]);
        throw std::runtime_error("Usage: DesertRoad.exe [--self-test | --capture NEW_DIRECTORY]");
    } catch (const std::exception& error) {
        std::fprintf(stderr,"DesertRoad: %s\n",error.what());
        return 1;
    }
}
