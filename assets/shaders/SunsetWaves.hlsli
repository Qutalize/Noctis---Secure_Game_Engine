float3 WaveSurface(float2 seaPosition, float time)
{
    const float2 primary = float2(0.57f, 1.79f);
    const float2 secondary = float2(-1.97f, 1.23f);
    const float2 chop = float2(3.71f, 2.63f);
    const float2 warp = float2(0.19f, -0.24f);
    const float drift = dot(seaPosition, warp) + time * 0.22f;
    const float phase = dot(seaPosition, primary) - 1.38f * time + 1.9f * sin(drift);
    const float side = dot(seaPosition, secondary) - 1.77f * time + 1.30f + 1.15f * cos(drift);
    const float cap = dot(seaPosition, chop) - 2.21f * time + 2.26f;
    const float height = 0.118f * sin(phase) + 0.069f * sin(side) + 0.028f * sin(cap);
    const float2 slope = 0.118f * cos(phase) * (primary + 1.9f * cos(drift) * warp) +
        0.069f * cos(side) * (secondary - 1.15f * sin(drift) * warp) +
        0.028f * cos(cap) * chop;
    return float3(height, slope);
}
