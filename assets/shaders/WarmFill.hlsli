float3 WarmFill(float4 light, float3 position, float3 normal)
{
    const float3 offset = light.xyz - position;
    const float squaredDistance = max(dot(offset, offset), .01f);
    const float incidence = saturate(dot(offset * rsqrt(squaredDistance), normal));
    return float3(1.f, .58f, .26f) * (light.w * incidence / (1.f + .3f * squaredDistance));
}
