
// Fragment shader entry point - Executes once per pixel
[[shader("fragment")]]
float4 PSMain(VSOutput input) : SV_TARGET
{
    // === MATERIAL PROPERTY SAMPLING ===
    // Sample base color texture and apply material color factor
    float4 baseColor = baseColorMap.Sample(baseColorSampler, input.UV) * material.baseColorFactor;

    // Sample metallic-roughness texture (metallic=B channel, roughness=G channel)
    // glTF standard: metallic stored in blue, roughness in green
    float2 metallicRoughness = metallicRoughnessMap.Sample(metallicRoughnessSampler, input.UV).bg;
    float metallic = metallicRoughness.x * material.metallicFactor;
    float roughness = metallicRoughness.y * material.roughnessFactor;

    // Sample ambient occlusion (typically stored in red channel)
    float ao = occlusionMap.Sample(occlusionSampler, input.UV).r;

    // Sample emissive texture for self-illuminating materials
    float3 emissive = emissiveMap.Sample(emissiveSampler, input.UV).rgb;

    // === NORMAL CALCULATION ===
    // Start with interpolated surface normal
    float3 N = normalize(input.Normal);

    // Apply normal mapping if texture is available
    if (material.normalTextureSet >= 0) {
        // Sample normal map and convert from [0,1] to [-1,1] range
        float3 tangentNormal = normalMap.Sample(normalSampler, input.UV).xyz * 2.0 - 1.0;

        // Construct tangent-space to world-space transformation matrix (TBN)
        float3 T = normalize(input.Tangent.xyz);              // Tangent
        float3 B = normalize(cross(N, T)) * input.Tangent.w;  // Bitangent (w = handedness)
        float3x3 TBN = float3x3(T, B, N);                     // Tangent-Bitangent-Normal matrix

        // Transform normal from tangent space to world space
        N = normalize(mul(tangentNormal, TBN));
    }

    // === LIGHTING SETUP ===
    // Calculate view direction (fragment to camera)
    float3 V = normalize(ubo.camPos.xyz - input.WorldPos);

    // Calculate reflection vector for environment mapping
    float3 R = reflect(-V, N);

    // === PBR MATERIAL SETUP ===
    // Calculate F0 (reflectance at normal incidence)
    // Non-metals: low reflectance (~0.04), Metals: colored reflectance from base color
    float3 F0 = float3(0.04, 0.04, 0.04);  // Dielectric default
    F0 = lerp(F0, baseColor.rgb, metallic); // Lerp to metallic behavior

    // Initialize outgoing radiance accumulator
    float3 Lo = float3(0.0, 0.0, 0.0);

    // === DIRECT LIGHTING LOOP ===
    // Calculate contribution from each light source
    for (int i = 0; i < 4; i++) {
        float3 lightPos = ubo.lightPositions[i].xyz;
        float3 lightColor = ubo.lightColors[i].rgb;

        // Calculate light direction and attenuation
        float3 L = normalize(lightPos - input.WorldPos);      // Light direction
        float distance = length(lightPos - input.WorldPos);   // Distance for falloff
        float attenuation = 1.0 / (distance * distance);     // Inverse square falloff
        float3 radiance = lightColor * attenuation;           // Attenuated light color

        // Calculate half vector (between view and light directions)
        float3 H = normalize(V + L);

        // === BRDF EVALUATION ===
        // Calculate all necessary dot products for BRDF terms
        float NdotL = max(dot(N, L), 0.0);  // Lambertian falloff
        float NdotV = max(dot(N, V), 0.0);  // View angle
        float NdotH = max(dot(N, H), 0.0);  // Half vector for specular
        float HdotV = max(dot(H, V), 0.0);  // For Fresnel calculation

        // Evaluate Cook-Torrance BRDF components
        float D = DistributionGGX(NdotH, roughness);    // Normal distribution
        float G = GeometrySmith(NdotV, NdotL, roughness); // Geometry function
        float3 F = FresnelSchlick(HdotV, F0);           // Fresnel reflectance

        // Calculate specular BRDF
        float3 numerator = D * G * F;
        float denominator = 4.0 * NdotV * NdotL + 0.0001; // Prevent division by zero
        float3 specular = numerator / denominator;

        // === ENERGY CONSERVATION ===
        // Fresnel term represents specular reflection ratio
        float3 kS = F;                          // Specular contribution
        float3 kD = float3(1.0, 1.0, 1.0) - kS; // Diffuse contribution (energy conservation)
        kD *= 1.0 - metallic;                   // Metals have no diffuse reflection

        // === RADIANCE ACCUMULATION ===
        // Combine diffuse (Lambertian) and specular (Cook-Torrance) terms
        // Multiply by incident radiance and cosine foreshortening
        Lo += (kD * baseColor.rgb / PI + specular) * radiance * NdotL;
    }

    // === AMBIENT AND EMISSIVE ===
    // Add simple ambient lighting (should be replaced with IBL in production)
    float3 ambient = float3(0.03, 0.03, 0.03) * baseColor.rgb * ao;

    // Combine all lighting contributions
    float3 color = ambient + Lo + emissive;

    // === HDR TONE MAPPING AND GAMMA CORRECTION ===
    // Apply Reinhard tone mapping to compress HDR values to [0,1] range
    color = color / (color + float3(1.0, 1.0, 1.0));

    // Apply gamma correction for sRGB display (inverse gamma)
    color = pow(color, float3(1.0 / ubo.gamma, 1.0 / ubo.gamma, 1.0 / ubo.gamma));

    // Output final color with original alpha
    return float4(color, baseColor.a);
}