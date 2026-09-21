#include "pbrCommon.slang"
[[vk::push_constant]] ConstantBuffer<PbrMaterial> material;
[[vk::binding(1, 0)]] Sampler2D baseColorMap;   //  binding 1, set 0
[[vk::binding(2, 0)]] Sampler2D metallicRoughnessMap;
[[vk::binding(3, 0)]] Sampler2D normalMap;
[[vk::binding(4, 0)]] Sampler2D occlusionMap;
[[vk::binding(5, 0)]] Sampler2D emissiveMap;
static const float PI = 3.14159265359;
float DistributionGGX(float NdotH, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float d = NdotH * NdotH * (a2 - 1.0) + 1.0;
    return a2 / max(PI * d * d, 1e-7);
}
float GeometrySmith(float NdotV, float NdotL, float roughness) {
    float r = roughness + 1.0;
    float k = r * r / 8.0;
    return (NdotV / max(NdotV * (1.0 - k) + k, 1e-5)) *
           (NdotL / max(NdotL * (1.0 - k) + k, 1e-5));
}
float3 FresnelSchlick(float cosTheta, float3 F0) {
    return F0 + (1.0 - F0) * pow(1.0 - saturate(cosTheta), 5.0);
}
[shader("fragment")]
float4 PSMain(VSOutput input, bool frontFace : SV_IsFrontFace) : SV_Target {
    // sRGB textures are decoded by the image format; data maps remain linear.
    float4 baseColor = baseColorMap.Sample(input.UV) * material.baseColorFactor;
    if (material.flags.y == 1 && baseColor.a < material.surface.w) discard;
    float2 mr = metallicRoughnessMap.Sample(input.UV).bg;
    float metallic = saturate(mr.x * material.surface.x);
    float roughness = clamp(mr.y * material.surface.y, 0.045, 1.0);
    float ao = lerp(1.0, occlusionMap.Sample(input.UV).r, material.surface.z);
    float3 emissive = emissiveMap.Sample(input.UV).rgb * material.emissiveNormal.xyz;
    float3 N = normalize(input.Normal);
    float3 T = normalize(input.Tangent.xyz - N * dot(N, input.Tangent.xyz));
    float3 B = cross(N, T) * input.Tangent.w;
    if (material.flags.x != 0) {
        float3 n = normalMap.Sample(input.UV).xyz * 2.0 - 1.0;
        n.xy *= material.emissiveNormal.w;
        N = normalize(T * n.x + B * n.y + N * n.z);
    }
    if (material.flags.z != 0 && !frontFace) N = -N;
    float3 V = normalize(ubo.camPos.xyz - input.WorldPos);
    float3 F0 = lerp(float3(0.04), baseColor.rgb, metallic);
    float3 Lo = float3(0.0);
    for (int i = 0; i < 4; ++i) {
        float3 delta = ubo.lightPositions[i].xyz - input.WorldPos;
        float distance2 = max(dot(delta, delta), 1e-4);
        float3 L = delta * rsqrt(distance2);
        float3 H = normalize(V + L + float3(1e-7));
        float NdotL = saturate(dot(N, L));
        float NdotV = saturate(dot(N, V));
        float3 F = FresnelSchlick(saturate(dot(H, V)), F0);
        float D = DistributionGGX(saturate(dot(N, H)), roughness);
        float G = GeometrySmith(NdotV, NdotL, roughness);
        float3 specular = D * G * F / max(4.0 * NdotV * NdotL, 1e-4);
        float3 diffuse = (1.0 - F) * (1.0 - metallic) * baseColor.rgb / PI;
        Lo += (diffuse + specular) * (ubo.lightColors[i].rgb / distance2) * NdotL;
    }
    // Simple ambient approximation, not image-based lighting.
    float3 color = Lo + ubo.parameters.w * baseColor.rgb * ao + emissive;
    color *= ubo.parameters.x;
    color = color / (color + 1.0);
    // sRGB swapchain attachments perform encoding on store: never apply gamma twice.
    if (ubo.parameters.z < 0.5) color = pow(color, float3(1.0 / ubo.parameters.y));
    return float4(color, material.flags.y == 2 ? baseColor.a : 1.0);
}
