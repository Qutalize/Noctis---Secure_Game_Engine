#include "game/desert/DesertScene.h"
#include "engine/core/Json.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <numbers>
#include <stdexcept>

using namespace DirectX;

namespace desert {
namespace {
Mesh Sky(Renderer& renderer) {
    constexpr int rings=18, segments=54;
    constexpr float radius=380.f, pi=std::numbers::pi_v<float>;
    std::vector<PbrVertex> vertices;
    std::vector<uint32_t> triangles;
    for (int row=0; row<=rings; ++row) {
        const float latitude=pi*(static_cast<float>(row)/rings-.5f);
        for (int column=0; column<=segments; ++column) {
            const float longitude=2.f*pi*column/segments;
            const XMFLOAT3 direction{std::sin(longitude)*std::cos(latitude),std::sin(latitude),
                                    std::cos(longitude)*std::cos(latitude)};
            vertices.push_back({{radius*direction.x,radius*direction.y,radius*direction.z},
                                {0,1,0},{1,0,0,1},{0,0}});
            if (row<rings && column<segments) {
                const auto index=static_cast<uint32_t>(row*(segments+1)+column);
                const auto next=index+segments+1;
                triangles.insert(triangles.end(),{index,next,index+1,index+1,next,next+1});
            }
        }
    }
    return renderer.CreatePbrMesh(vertices,triangles);
}
}

Route::Route(const std::filesystem::path& file) {
    std::ifstream stream(file,std::ios::binary|std::ios::ate);
    const auto size=stream.tellg();
    if (size<=0 || size>8192) throw std::runtime_error("Missing or invalid desert route");
    std::string data(static_cast<std::size_t>(size),'\0');
    stream.seekg(0);
    json::Value root;
    if (!stream.read(data.data(),static_cast<std::streamsize>(data.size())) ||
        !json::Parse(data.data(),data.size(),root,nullptr)) throw std::runtime_error("Cannot parse desert route");
    const auto& keys=root["road"];
    if (!keys.IsArray() || keys.Size()<2 || keys.Size()>64) throw std::runtime_error("Invalid road key count");
    for (std::size_t index=0; index<keys.Size(); ++index) {
        const auto& row=keys[index];
        if (!row.IsArray() || row.Size()!=3 || !row[0].IsNumber() || !row[1].IsNumber() || !row[2].IsNumber()) {
            throw std::runtime_error("Invalid road key");
        }
        const XMFLOAT3 key{row[0].Flt(), row[1].Flt(), row[2].Flt()};
        if (!std::isfinite(key.x) || !std::isfinite(key.y) || !std::isfinite(key.z) ||
            (!m_keys.empty() && key.x<=m_keys.back().x)) throw std::runtime_error("Unordered road route");
        m_keys.push_back(key);
    }
}

XMFLOAT2 Route::At(float distance) const {
    const auto found=std::upper_bound(m_keys.begin(),m_keys.end(),distance,
        [](float value,const XMFLOAT3& key){ return value<key.x; });
    if (found==m_keys.begin()) return {found->y,found->z};
    if (found==m_keys.end()) return {m_keys.back().y,m_keys.back().z};
    const auto& start=*(found-1);
    const float weight=(distance-start.x)/(found->x-start.x);
    return {std::lerp(start.y,found->y,weight),std::lerp(start.z,found->z,weight)};
}

XMFLOAT3 Route::Point(float distance,float offset,float height) const {
    const auto road=At(distance);
    return {road.x+offset,road.y+height,distance};
}

XMFLOAT2 Route::Step(XMFLOAT2 position,XMFLOAT2 delta) const {
    const float distance=std::clamp(position.y+delta.y,-29.f,kRoadWalkEnd);
    const float axis=At(distance).x;
    return {std::clamp(position.x+delta.x,axis-4.62f,axis+4.62f),distance};
}

Scene::Scene(Renderer& renderer,const std::filesystem::path& assets) : m_route(assets/"route.json") {
    const auto load=[&](GltfProp& prop,const char* name) {
        const auto file=(assets/name/(std::string(name)+"_1k.gltf")).u8string();
        if (!prop.Load(renderer,std::string(file.begin(),file.end()),true,true)) {
            throw std::runtime_error(std::string("Missing desert asset: ")+name);
        }
    };
    load(m_ground,"road_corridor");
    load(m_mesas,"sandstone_mesas");
    load(m_props,"roadside");
    load(m_rock,"rock_face_01");
    m_roadsidePerson.Load(renderer, assets, "taylor");
    m_sky=Sky(renderer);
    m_skyMaterial=renderer.CreatePbrMaterial(nullptr,nullptr,nullptr);
}

void Scene::Draw(Renderer& renderer,const Camera& camera,float time,float evening,float verticalFov) {
    const float elevation=std::lerp(.31f,.123f,evening);
    const float azimuth=std::lerp(.52f,.43f,evening);
    const XMFLOAT3 sun{std::sin(azimuth)*std::cos(elevation),std::sin(elevation),std::cos(azimuth)*std::cos(elevation)};
    renderer.SetFog(false);
    renderer.SetCoastalHaze({.64f,.55f,.47f},.00145f);
    renderer.SetDirectionalLight({-sun.x,-sun.y,-sun.z},{1.f,.74f,.46f},1.55f,{.61f,.56f,.50f});
    renderer.SetShadowExtent(86.f);
    renderer.BeginFrame(camera,verticalFov,1400.f);
    const auto& eye=camera.GetPosition();
    m_skyMaterial.coastalParams={2.f,time,std::lerp(.18f,.69f,evening),0.f};
    renderer.DrawPbrMesh(m_sky,m_skyMaterial,XMMatrixTranslation(eye.x,eye.y,eye.z),true);
    const auto identity=XMMatrixIdentity();
    m_ground.Draw(renderer,identity);
    m_mesas.Draw(renderer,identity);
    m_props.Draw(renderer,identity);
    m_roadsidePerson.Draw(renderer, camera, m_route.Point(51.7f, 9.45f, .25f), -.2f);
    m_roadsidePerson.Draw(renderer, camera, m_route.Point(49.7f, 7.03f, .02f), 1.92f);
    for (const auto& placement : {std::array{-17.3f,37.f,1.50f,.28f},std::array{14.8f,62.f,1.30f,-1.2f},
             std::array{-22.5f,90.f,2.2f,.65f},std::array{16.5f,104.f,1.36f,.20f},
             std::array{-20.4f,144.f,1.65f,-.22f},std::array{13.6f,181.f,2.1f,.43f}}) {
        const auto [x,z,scale,rotation]=placement;
        m_rock.Draw(renderer,XMMatrixScaling(scale,scale,scale)*XMMatrixRotationY(rotation)*
                             XMMatrixTranslation(x,m_route.At(z).y-1.42f,z));
    }
}
}
