float3 WaveSurface(float2 point, float time)
{
    const float2 primary = float2(0.57f, 1.79f);
    const float2 secondary = float2(-1.97f, 1.23f);
    const float2 chop = float2(3.71f, 2.63f);
    const float phase = dot(point, primary) - 1.38f * time;
    const float side = dot(point, secondary) - 1.77f * time + 1.30f;
    const float cap = dot(point, chop) - 2.21f * time + 2.26f;
    const float height = 0.118f * sin(phase) + 0.069f * sin(side) + 0.028f * sin(cap);
    const float2 slope = 0.118f * cos(phase) * primary + 0.069f * cos(side) * secondary +
        0.028f * cos(cap) * chop;
    return float3(height, slope);
}
