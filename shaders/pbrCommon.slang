// Shared CPU/GPU layout: src/pbr.h. Compiled with -matrix-layout-column-major.
struct PbrUniforms {
    float4x4 model;
    float4x4 view;
    float4x4 proj;
    float4x4 normalMatrix;
    float4 lightPositions[4];
    float4 lightColors[4];
    float4 camPos;
    float4 parameters;
};
struct PbrMaterial {
    float4 baseColorFactor;
    float4 emissiveNormal;
    float4 surface;
    int4 flags;
};
[[vk::binding(0, 0)]] ConstantBuffer<PbrUniforms> ubo;      // ubo数据 从 set 0、binding 0 对应的缓冲区读取
struct VSOutput {
    float4 Position : SV_Position;
    [[vk::location(0)]] float3 WorldPos : TEXCOORD0;
    [[vk::location(1)]] float3 Normal : TEXCOORD1;
    [[vk::location(2)]] float2 UV : TEXCOORD2;
    [[vk::location(3)]] float4 Tangent : TEXCOORD3;
};
