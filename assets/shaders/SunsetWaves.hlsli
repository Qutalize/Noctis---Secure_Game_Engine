float CoastalHash(float2 cell)
{
    float3 value = frac(float3(cell.xyx) * float3(.1031f, .1030f, .0973f));
    value += dot(value, value.yzx + 33.33f);
    return frac((value.x + value.y) * value.z);
}

float CoastalNoise(float2 uv)
{
    float2 cell = floor(uv);
    float2 blend = frac(uv);
    blend = blend * blend * (3.0f - 2.0f * blend);
    return lerp(lerp(CoastalHash(cell), CoastalHash(cell + float2(1, 0)), blend.x),
                lerp(CoastalHash(cell + float2(0, 1)), CoastalHash(cell + 1), blend.x), blend.y);
}

float Shoreline(float x)
{
    return -11.0f + 1.55f * sin(.17f * x) + .54f * sin(.39f * x + 1.4f);
}

float2 FineRipples(float2 uv, float time)
{
    float2 motion = uv - float2(.13f, .39f) * time;
    float2 drift = float2(CoastalNoise(motion * .38f), CoastalNoise(motion.yx * .59f));
    float2 field = motion * float2(5.4f, 10.6f) + drift * 2.6f;
    float footprint = max(length(ddx(field)), length(ddy(field)));
    float2 ripples = float2(CoastalNoise(field), CoastalNoise(field.yx + 5.7f)) - .5f;
    return ripples * (1.f - smoothstep(.28f, 1.25f, footprint));
}

float3 CoastalBottom(float2 seaPosition, float depth, float3 view, float2 slope, float time, float daylight)
{
    float path = depth / max(.28f, view.y);
    float2 bottomUV = seaPosition - path * (view.xz * .56f + slope * .46f);
    float2 rippleUV = bottomUV * float2(1.8f, 2.7f);
    float2 shift = float2(CoastalNoise(rippleUV * .72f - .32f * time),
                          CoastalNoise(rippleUV.yx * .81f + .21f * time));
    float caustic = 1.f - smoothstep(.035f, .16f, abs(shift.x - shift.y));
    float footprint = max(length(ddx(rippleUV)), length(ddy(rippleUV)));
    caustic *= (1.f - smoothstep(.24f, .95f, footprint)) * exp(-.63f * depth);
    float sand = CoastalNoise(bottomUV * 1.14f) * .65f + CoastalNoise(bottomUV * 4.3f) * .35f;
    float3 bed = lerp(float3(.54f, .47f, .30f), float3(.85f, .75f, .54f), sand);
    float3 transport = exp(-path * float3(.98f, .30f, .19f));
    float3 scatter = float3(.071f, .39f, .46f) * daylight;
    return bed * (daylight + .18f * caustic * daylight) * transport + scatter * (1.f - transport);
}

float WaveSurface(float2 seaPosition, float time)
{
    float2 wind = seaPosition - time * float2(.18f, .41f);
    float phase = CoastalNoise(wind * .16f) * 4.5f;
    float swell = sin(dot(wind, float2(.34f, 1.34f)) + phase);
    float crossing = sin(dot(wind, float2(-1.05f, 1.81f)) - phase * 1.6f);
    return .038f * swell + .019f * crossing + .029f * (CoastalNoise(wind * .61f) - .5f);
}

float3 CoastalSky(float3 direction, float3 sun, float sunset, float dusk, float time)
{
    float elevation = 1.0f - exp(-max(direction.y, 0.0f) * 3.8f);
    float3 high = lerp(float3(.12f, .45f, .72f), float3(.20f, .25f, .49f), sunset);
    high = lerp(high, float3(.031f, .075f, .18f), dusk);
    float3 low = lerp(float3(.61f, .81f, .90f), float3(.97f, .49f, .29f), sunset);
    low = lerp(low, float3(.13f, .19f, .33f), dusk);
    float alignment = saturate(dot(direction, sun));
    float3 glow = float3(1.0f, .54f, .24f) * (pow(alignment, 38.0f) * .34f + pow(alignment, 210.0f) * .52f);
    float2 cloudUV = direction.xz * (2.5f / (max(direction.y, .015f) + .27f));
    cloudUV += time * float2(.013f, -.008f);
    float cloud = CoastalNoise(cloudUV) * .58f + CoastalNoise(cloudUV * 2.16f) * .27f +
        CoastalNoise(cloudUV * 4.43f) * .11f + CoastalNoise(cloudUV * 8.63f) * .04f;
    float cover = smoothstep(.47f, .71f, cloud) * smoothstep(.015f, .15f, direction.y);
    float3 cloudColor = lerp(float3(.73f, .83f, .88f), float3(.49f, .29f, .36f), sunset);
    cloudColor += float3(.34f, .22f, .10f) * pow(alignment, 9.0f) * sunset;
    cloudColor = lerp(cloudColor, float3(.09f, .13f, .25f), dusk);
    float disk = smoothstep(.99964f, .99985f, alignment);
    return lerp(lerp(low, high, elevation) + glow * (1.0f - dusk), cloudColor, cover * .88f) +
        disk * float3(1.0f, .85f, .58f) * smoothstep(-.025f, .025f, sun.y) * (1.0f - cover * .7f);
}
