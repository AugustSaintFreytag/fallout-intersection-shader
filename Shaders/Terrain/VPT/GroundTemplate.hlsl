// Ground mode pixel shader for the terrain overlay, for blending up to 7 textures and using up to 24 pointlights.
// Shading from the terrain template of Vanilla Plus Terrain. The plugin draws it in place of the terrain pixel shader
// of each replayed landscape pass.
// Reference at "Shaders/Includes/TerrainOverlay.hlsli".

#if defined(__INTELLISENSE__)
    #define NUM_PT_LIGHTS 24
#endif

#ifndef TEX_COUNT
    #define TEX_COUNT 1
#endif

#if defined(NUM_PT_LIGHTS) && (NUM_PT_LIGHTS > 24)
    #define NUM_PT_LIGHTS 24
#endif

#include "includes/Terrain.hlsli"

struct PS_INPUT {
    float2 vpos : VPOS;
};

struct PS_OUTPUT {
    float4 color_0 : COLOR0;
};

sampler2D BaseMap[7]:register(s0);
sampler2D NormalMap[7]:register(s7);

float4 AmbientColor : register(c1);
float4 PSLightColor[8] : register(c3);
float4 PSLightDir : register(c18);
float4 PSLightPosition[7] : register(c19);

float4 LandSpec[2] : register(c32);
float4 LandHeight[2] : register(c34);
float4 FogParam : register(c36);
float4 FogColor : register(c37);

#ifdef NUM_PT_LIGHTS
float4 PointLightColor[NUM_PT_LIGHTS] : register(c39);
float4 PointLightPosition[NUM_PT_LIGHTS] : register(c63);
float  PointLightCount : register(c88);
#endif

#include "../../Includes/TerrainOverlay.hlsli"

PS_OUTPUT main(PS_INPUT IN) {
    PS_OUTPUT OUT = (PS_OUTPUT)0;

    // Leave before any shading. The pixel belongs to another block or gets no overlay.
    GroundSample ground;
    [branch] if (!GetGroundSample(IN.vpos, ground)) {
        clip(-1.0f);
        return OUT;
    }

    float3 tangent;
    float3 binormal;
    float3 normal;
    GetGroundBasis(ground.groundNormal, tangent, binormal, normal);

    int texCount = TEX_COUNT;
    float3x3 tbn = float3x3(tangent, binormal, normal);
    float3 lPosition = ground.position;
    float3 eyePosition = IBModelEye.xyz;
    float3 eyeDir = normalize(mul(tbn, eyePosition - lPosition));
    float2 dx = ground.uvDx;
    float2 dy = ground.uvDy;

    float blends[7] = { ground.blend0.x, ground.blend0.y, ground.blend0.z, ground.blend0.w, ground.blend1.x, ground.blend1.y, ground.blend1.z };
    float spec[7] = { LandSpec[0].x, LandSpec[0].y, LandSpec[0].z, LandSpec[0].w, LandSpec[1].x, LandSpec[1].y, LandSpec[1].z };

    float gloss = 0.0f;
    float specExponent = 0.0f;
    float3 baseColor = blendDiffuseMaps(ground.vertexColor, ground.uv, dx, dy, texCount, BaseMap, blends);
    float3 combinedNormal = blendNormalMaps(ground.uv, dx, dy, texCount, NormalMap, blends, spec, gloss, specExponent);

    float3 lightTS = mul(tbn, PSLightDir.xyz);

    float3 lighting = getDirectionalLighting(lightTS, PSLightColor[0].rgb, eyeDir, combinedNormal, baseColor, gloss, specExponent);
    lighting += getAmbientLighting(baseColor, AmbientColor.rgb);

    #if defined(NUM_PT_LIGHTS)
        [loop] for (int i = 0; i < PointLightCount; i++) {
            float3 pointlightDir = PointLightPosition[i].xyz - lPosition;
            float att = attenuation(pointlightDir, PointLightPosition[i].w);

            [branch]
            if (att > 0.001) {
                pointlightDir = mul(tbn, pointlightDir);
                lighting += getPointLightingAtt(pointlightDir, att, PointLightColor[i].rgb, eyeDir, combinedNormal, baseColor, gloss, specExponent);
            }
        }
    #endif

    // Per pixel fog.
    float4 fog;
    float3 fogPos = GetFogPosition(lPosition);
    float fogStrength = 1 - saturate((FogParam.x - length(fogPos)) / FogParam.y);
    fog.rgb = FogColor.rgb;
    fog.a = pow(fogStrength, FogParam.z);

    float3 finalColor = lighting;
    finalColor = lerp(finalColor, fog.rgb, fog.a); // Apply fog.

    if (IsOverlayMaskDebug())
        finalColor = GetBlockColor(IBBlock.xy);

    OUT.color_0.a = ground.alpha;
    OUT.color_0.rgb = finalColor;

    return OUT;
};
