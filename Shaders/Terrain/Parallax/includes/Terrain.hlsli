#if defined(__INTELLISENSE__)
    #include "Helpers.hlsli"
    #include "Pointlights.hlsli"
#else
    #include "Pointlights.hlsli"
#endif

// Blends the diffuse maps by their weights and tints the result with the vertex colour.
// Samples with explicit texture coordinate gradients. The terrain overlay shifts the coordinates per pixel.
float3 blendDiffuseMaps(float3 vertexColor, float2 uv, float2 dx, float2 dy, int texCount, sampler2D tex[7], float blends[7]) {
    float3 color = float3(0, 0, 0);
    [unroll]
    for (int i = 0; i < texCount; i++) {
        color += tex2Dgrad(tex[i], uv, dx, dy).xyz * blends[i];
    }

    return color * vertexColor;
}

float3 blendNormalMaps(float2 uv, float2 dx, float2 dy, int texCount, sampler2D tex[7], float blends[7], float spec[7], out float gloss, out float specExponent) {
    gloss = 0.0f;
    specExponent = 0.0f;

    float3 blendedNormal = float3(0, 0, 0);

    float4 normal;
    [unroll]
    for (int i = 0; i < texCount; i++) {
        normal = tex2Dgrad(tex[i], uv, dx, dy);
        blendedNormal += normal.xyz * blends[i];
        gloss += normal.w * blends[i] * (spec[i] > 0 ? 1.0f : 0.0f);
        specExponent += spec[i] * blends[i];
    }

    gloss = saturate(gloss);
    return normalize(expand(blendedNormal));
}

float3 getVanillaLightingAtt(float3 lightDir, float att, float3 lightColor, float3 viewDir, float3 normal, float3 albedo, float gloss, float glossPower) {
    lightDir = normalize(lightDir);
    viewDir = normalize(viewDir);
    float3 halfwayDir = normalize(lightDir + viewDir);
    
    float NdotL = shades(normal.xyz, lightDir.xyz);
    
    float specStrength = gloss * pow(abs(shades(normal.xyz, halfwayDir.xyz)), glossPower);
    float3 lighting = albedo.rgb * NdotL * lightColor.rgb * att;
    lighting += saturate(((0.2 >= NdotL ? (specStrength * saturate(NdotL + 0.5)) : specStrength) * lightColor.rgb) * att);
    
    return lighting;
}

float3 getPointLightLighting(float3 lightDir, float att, float3 lightColor, float3 eyeDir, float3 normal, float3 albedo, float gloss = 0.0, float glossPower = 0.0, float metallicness = 1.0) {
    float3 pointlightColor = lightColor;

    lightDir = normalize(lightDir);
        
    float3 lighting = getVanillaLightingAtt(lightDir, att, lightColor, eyeDir, normal, albedo, gloss, glossPower);
        
    return lighting;
}

float3 getSunLighting(float3 lightDir, float3 sunColor, float3 eyeDir, float3 normal, float3 AmbientColor, float3 albedo, float gloss = 0.0, float glossPower = 0.0, float metallicness = 1.0, float parallaxMultiplier = 1.0) {
    float3 lightColor = sunColor * parallaxMultiplier;
    float3 ambientColor = AmbientColor;
    float3 color = albedo;

    float3 lighting = getVanillaLightingAtt(lightDir, 1.0, sunColor, eyeDir, normal, albedo, gloss, glossPower);
        
    return lighting + ambientColor * color;
}
