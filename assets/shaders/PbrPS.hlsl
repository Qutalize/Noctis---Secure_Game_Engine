#include "RayTracing.hlsli"
#ifndef ENGINE_SHADOWS
#define ENGINE_SHADOWS 1
#endif
// PBR ピクセルシェーダー（ステージL-02）。Cook-Torrance の金属/粗さワークフロー。
//
// Blinn-Phong（既存の SimplePS）との違い:
//   Blinn-Phong は「拡散 + 適当な鏡面」を足すだけで、物理的な裏付けが無い。
//   結果として、金属も布も「色が違うだけ」に見える＝素材感が出ない。
//   PBR は「入ってきた光は反射か拡散のどちらかになり、合計は入射量を超えない」
//   というエネルギー保存を守るので、metallic / roughness の 2 つのパラメータだけで
//   金属・プラスチック・布・皮革が描き分けられる。CC0 アセットもこの前提で作られている。
//
// テクスチャ:
//   t0 = baseColor（アルベド）
//   t1 = normal（接空間の法線マップ）
//   t2 = ORM（R=遮蔽 AO / G=粗さ roughness / B=金属度 metallic）※glTF の標準的な詰め方
//   t3 = シャドウマップ

cbuffer SceneConstants : register(b0)
{
    float4x4 gView;
    float4x4 gProj;
    float4x4 gWorld;
    float4   gLightDir;
    float4   gLightColor;
    float4   gAmbient;
    float4   gCameraPos;
    float4x4 gLightViewProj;
    float4   gShadowParams;
    float4   gBaseColorFactor;
    float4   gMaterialParams;   // x=metallic, y=roughness, z=法線マップ強度, w=テクスチャフラグ
    float4 gCoastalParams;
    float4 gFillLight;
    float4 gAccentLight;
    float4 gRearLight;
};

#include "SunsetWaves.hlsli"
#include "WarmFill.hlsli"

Texture2D gBaseColorMap : register(t0);
Texture2D gNormalMap    : register(t1);
Texture2D gOrmMap       : register(t2);
SamplerState gSampler   : register(s0);

Texture2D              gShadowMap     : register(t3);
SamplerComparisonState gShadowSampler : register(s1);

struct PSInput
{
    float4 position  : SV_Position;
    float3 worldPos  : POSITION;
    float3 normal    : NORMAL;
    float3 tangent   : TANGENT;
    float3 bitangent : BINORMAL;
    float2 uv        : TEXCOORD;
};

static const float PI = 3.14159265f;

// ---- 影（SimplePS と同じ内容）-------------------------------------------
float ComputeShadow(float3 worldPos, float ndotl)
{
#if ENGINE_SHADOWS

    if (gShadowParams.z < 0.5f) return 1.0f;
    float4 lp = mul(float4(worldPos, 1.0f), gLightViewProj);
    lp.xyz /= lp.w;
    float2 uv = lp.xy * float2(0.5f, -0.5f) + 0.5f;
    if (uv.x < 0.0f || uv.x > 1.0f || uv.y < 0.0f || uv.y > 1.0f || lp.z > 1.0f) return 1.0f;
    float bias     = gShadowParams.y * (1.0f + 2.0f * (1.0f - ndotl));
    float refDepth = lp.z - bias;
    float texel = gShadowParams.x;
    float sum   = 0.0f;
    [unroll] for (int y = -1; y <= 1; ++y) {
        [unroll] for (int x = -1; x <= 1; ++x) {
            sum += gShadowMap.SampleCmpLevelZero(
                gShadowSampler, uv + float2(x, y) * texel, refDepth);
        }
    }
    return sum / 9.0f;

#else
    return 1.0f;
#endif
}

// ---- Cook-Torrance の 3 要素 --------------------------------------------

// D項: 法線分布関数（GGX / Trowbridge-Reitz）
// 「микро表面のうち、どれだけがハーフベクトル方向を向いているか」＝ハイライトの形。
// roughness が小さいほど鋭く、大きいほど広く鈍いハイライトになる。
float DistributionGGX(float ndoth, float roughness)
{
    float a  = roughness * roughness;
    float a2 = a * a;
    float d  = ndoth * ndoth * (a2 - 1.0f) + 1.0f;
    return a2 / max(PI * d * d, 1e-6f);
}

// G項: 幾何減衰（Smith + Schlick-GGX）
// ざらついた表面で、微小な凹凸が互いに影を落とし合って暗くなる効果。
float GeometrySchlickGGX(float ndotv, float roughness)
{
    float r = roughness + 1.0f;
    float k = (r * r) / 8.0f;
    return ndotv / max(ndotv * (1.0f - k) + k, 1e-6f);
}
float GeometrySmith(float ndotv, float ndotl, float roughness)
{
    return GeometrySchlickGGX(ndotv, roughness) * GeometrySchlickGGX(ndotl, roughness);
}

// F項: フレネル（Schlick 近似）
// 「浅い角度から見るほど反射が強くなる」現象。水面を斜めから見ると鏡になるあれ。
// これがあるだけで一気に「それっぽく」なる。
float3 FresnelSchlick(float cosTheta, float3 f0)
{
    return f0 + (1.0f - f0) * pow(saturate(1.0f - cosTheta), 5.0f);
}

float4 PSMain(PSInput input) : SV_Target
{
    if (gCoastalParams.x > 2.5f) {
        return float4(gBaseColorFactor.rgb * lerp(.15f, 1.f, gCoastalParams.w), 1.0f);
    }
    if (gCoastalParams.x > 1.5f) {
        float3 direction = normalize(input.worldPos - gCameraPos.xyz);
        return float4(saturate(CoastalSky(direction, normalize(-gLightDir.xyz),
                                         gCoastalParams.z, gCoastalParams.w, gCoastalParams.y)), 1.0f);
    }
    if (gCoastalParams.x > 0.5f) {
        const float time = gCoastalParams.y;
        const float sunset = gCoastalParams.z;
        const float dusk = gCoastalParams.w;
        const float2 seaPosition = input.worldPos.xz;
        const float wave = WaveSurface(seaPosition, time);
        const float distanceToEye = length(gCameraPos.xyz - input.worldPos);
        const float horizon = smoothstep(30.0f, 145.0f, distanceToEye);
        const float2 wind = seaPosition - time * float2(.25f, .36f);
        const float2 patch = float2(CoastalNoise(wind * .19f), CoastalNoise(wind.yx * .31f));
        const float2 rippleUV = wind * float2(2.8f, 6.5f) + patch * 2.3f;
        const float footprint = max(length(ddx(rippleUV)), length(ddy(rippleUV)));
        const float rippleGain = .055f * (1.0f - smoothstep(.22f, 1.15f, footprint));
        const float2 ripple = float2(CoastalNoise(rippleUV) - .5f, CoastalNoise(rippleUV.yx + 3.41f) - .5f);
        const float2 gradient = float2(WaveSurface(seaPosition + float2(.13f, 0), time) - wave,
                                      WaveSurface(seaPosition + float2(0, .13f), time) - wave) / .13f;
        const float2 micro = FineRipples(seaPosition, time) * .055f;
        const float2 slope = (gradient * .59f + ripple * rippleGain + micro) * lerp(1.0f, .12f, horizon);
        const float3 normal = normalize(float3(-slope.x, 1.0f, -slope.y));
        const float3 view = normalize(gCameraPos.xyz - input.worldPos);
        const float3 reflected = reflect(-view, normal);
        const float3 sun = normalize(-gLightDir.xyz);
        const float fresnel = .025f + .82f * pow(1.0f - saturate(dot(normal, view)), 5.0f);
        const float3 sky = CoastalSky(normalize(float3(reflected.x, max(.02f, reflected.y), reflected.z)),
                                       sun, sunset, dusk, time);
        const float alignment = saturate(dot(reflected, sun));
        const float sparkle = lerp(.44f, 1.0f, CoastalNoise(rippleUV * .42f));
        const float glint = (pow(alignment, 100.0f) * .20f + pow(alignment, 580.0f) * sparkle * 1.25f) *
            (1.0f - dusk) * smoothstep(-.02f, .08f, sun.y);
        const float shore = seaPosition.y - Shoreline(seaPosition.x);
        const float runup = .22f + .47f * CoastalNoise(float2(seaPosition.x * .64f, time * .58f));
        const float foamLine = 1.0f - smoothstep(.048f, .26f, abs(shore - runup));
        const float wash = foamLine * smoothstep(.30f, .77f, CoastalNoise(wind * float2(3.8f, 5.1f) + patch));
        const float shadow = ComputeShadow(input.worldPos, saturate(dot(normal, sun)));
        const float depth = max(.024f, shore * (.21f + .047f * patch.x) + wave);
        const float daylight = lerp(1.f, .24f, dusk);
        const float3 bottom = CoastalBottom(seaPosition, depth, view, slope, time, daylight);
        const float3 tint = lerp(bottom, float3(.056f, .32f, .45f) * daylight,
                                 smoothstep(2.6f, 4.3f, depth));
        const float3 sea = lerp(tint, sky, fresnel) +
            shadow * glint * lerp(float3(.83f, .91f, 1.f), float3(1.f, .64f, .35f), sunset);
        const float3 foam = float3(.82f, .90f, .85f) * lerp(1.0f, .40f, dusk);
        const float3 color = lerp(sea, foam, wash * .72f);
        return float4(saturate(lerp(color, sky, .38f * horizon)), 1.0f);
    }
    const bool hasTextures = (gMaterialParams.w > 0.5f);

    // --- マテリアル値の取得 ---
    float4 albedo = gBaseColorFactor;
    float  metallic  = gMaterialParams.x;
    float  roughness = gMaterialParams.y;
    float  ao        = 1.0f;

    if (hasTextures) {
        albedo *= gBaseColorMap.Sample(gSampler, input.uv);
        // glTF の慣習: ORM テクスチャは R=AO / G=roughness / B=metallic。
        // 係数（gMaterialParams）はテクスチャに乗算する形で使う。
        float3 orm = gOrmMap.Sample(gSampler, input.uv).rgb;
        ao        = orm.r;
        roughness *= orm.g;
        metallic  *= orm.b;
    }
    // roughness が 0 だとハイライトが 1 ピクセルの点になって破綻するので下限を切る。
    roughness = clamp(roughness, 0.04f, 1.0f);
    metallic  = saturate(metallic);

    // --- 法線 ---
    float3 N = normalize(input.normal);
    if (hasTextures) {
        // 法線マップは [0,1] に詰められているので [-1,1] へ戻す。
        float3 tn = gNormalMap.Sample(gSampler, input.uv).xyz * 2.0f - 1.0f;
        tn.xy *= gMaterialParams.z;   // 強度（0 で法線マップ無効）
        float3x3 TBN = float3x3(normalize(input.tangent),
                                normalize(input.bitangent),
                                N);
        N = normalize(mul(tn, TBN));
    }

    float3 V = normalize(gCameraPos.xyz - input.worldPos);
    float3 L = normalize(-gLightDir.xyz);   // gLightDir は光が進む向き
    float3 H = normalize(V + L);

    float ndotl = saturate(dot(N, L));
    float ndotv = saturate(dot(N, V));
    float ndoth = saturate(dot(N, H));
    float vdoth = saturate(dot(V, H));

    // --- F0（垂直入射での反射率）---
    // 非金属はほぼ一律 0.04（4% だけ反射する）。金属は「アルベドの色で反射する」＝
    // 金や銅が色付いて光るのはこのため。metallic で 2 つを補間する。
    float3 f0 = lerp(float3(0.04f, 0.04f, 0.04f), albedo.rgb, metallic);

    float3 F = FresnelSchlick(vdoth, f0);
    float  D = DistributionGGX(ndoth, roughness);
    float  G = GeometrySmith(ndotv, ndotl, roughness);

    float3 specular = (D * G * F) / max(4.0f * ndotv * ndotl, 1e-4f);

    // エネルギー保存: 反射に使われなかったぶんだけが拡散に回る。
    // さらに金属は拡散反射をしない（入った光は全部吸収される）ので metallic で削る。
    float3 kd = (1.0f - F) * (1.0f - metallic);
    float3 diffuse = kd * albedo.rgb / PI;

    float  shadow   = ComputeShadow(input.worldPos, ndotl);
#if ENGINE_RT_SHADOWS
    shadow = RayShadow(input.worldPos,N,L);
#endif
    float3 radiance = gLightColor.rgb * gLightColor.w;
    float3 direct   = (diffuse + specular) * radiance * ndotl * shadow;

    // 環境光。本来は IBL（L-07）で方向ごとの環境輝度を使うべきところを、
    // 今は定数の環境光 × AO で代用している。金属が暗く沈みがちなのはこのため。
    float3 ambient = gAmbient.rgb * albedo.rgb * ao;

    float3 lamps = WarmFill(gFillLight, input.worldPos, N) + WarmFill(gAccentLight, input.worldPos, N) +
                   WarmFill(gRearLight, input.worldPos, N);
    float3 color = ambient + direct + albedo.rgb * lamps;
#if ENGINE_RT_REFLECTIONS
    color = lerp(color,RayReflection(input.worldPos,N,V,L,gLightColor.rgb*gLightColor.w,gAmbient.rgb),FresnelSchlick(ndotv,f0) * (1.0f - .7f * roughness));
#endif

    // 距離フォグ（既存 PS と同じ。gCameraPos.w=1 で有効）
    const float3 fogColor = float3(0.62f, 0.70f, 0.82f);
    const float  fogStart = 22.0f;
    const float  fogEnd   = 85.0f;
    float dist = length(gCameraPos.xyz - input.worldPos);
    float fog  = saturate((dist - fogStart) / (fogEnd - fogStart)) * gCameraPos.w;
    color = lerp(color, fogColor, fog);

    // ★暫定のトーンマップ★
    // PBR の計算結果は 1.0 を超える（HDR）。今はレンダーターゲットが 8bit なので、
    // ここで簡易的に 0..1 へ圧縮しないと明部が白飛びする。
    // L-06 で HDR レンダーターゲット + ACES トーンマップに置き換える。
    color = color / (color + 1.0f);
    color = pow(color, 1.0f / 2.2f);   // ガンマ補正（sRGB 相当）

    return float4(color, albedo.a);
}
