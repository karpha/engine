// Vertex shader entry point - Executes once per vertex
[[shader("vertex")]]
VSOutput VSMain(VSInput input)
{
    VSOutput output;

    // Transform vertex position through the rendering pipeline
    // Model -> World -> Camera -> Clip space transformation chain
    float4 worldPos = mul(ubo.model, float4(input.Position, 1.0));
    output.Position = mul(ubo.proj, mul(ubo.view, worldPos));

    // Pass world position for fragment lighting calculations
    // Fragment shader needs world space position to calculate light vectors
    output.WorldPos = worldPos.xyz;

    // Transform normal from model space to world space
    // Use only rotation/scale part of model matrix (upper-left 3x3)
    // Normalize to ensure unit length after transformation
    output.Normal = normalize(mul((float3x3)ubo.model, input.Normal));

    // Pass through texture coordinates unchanged
    // UV coordinates are typically in [0,1] range and don't need transformation
    output.UV = input.UV;

    // Pass tangent vector for normal mapping
    // Will be used in fragment shader to construct tangent-space basis
    output.Tangent = input.Tangent;

    return output;
}
