#include "pbrCommon.slang"
struct VSInput {
    [[vk::location(0)]] float3 Position : POSITION;
    [[vk::location(1)]] float3 Normal : NORMAL;
    [[vk::location(2)]] float2 UV : TEXCOORD0;
    [[vk::location(3)]] float4 Tangent : TANGENT;
};
[shader("vertex")]
VSOutput VSMain(VSInput input) {
    VSOutput output;
    float4 world = mul(ubo.model, float4(input.Position, 1.0));
    output.Position = mul(ubo.proj, mul(ubo.view, world));
    output.WorldPos = world.xyz;
    output.Normal = normalize(mul((float3x3)ubo.normalMatrix, input.Normal));
    output.Tangent = float4(mul((float3x3)ubo.model, input.Tangent.xyz),
                           input.Tangent.w * (determinant((float3x3)ubo.model) < 0.0 ? -1.0 : 1.0));
    output.UV = input.UV;
    return output;
}
