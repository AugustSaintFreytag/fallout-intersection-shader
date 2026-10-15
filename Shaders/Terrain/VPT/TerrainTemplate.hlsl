// Template for terrain shaders for blending up to 7 textures and using up to 12 pointlights.
// From Vanilla Plus Terrain, extended with the Intersection Blend overlay mode.
// Reference at "Shaders/Includes/TerrainOverlay.hlsli".

#if defined(__INTELLISENSE__)
    #define PS
    #define NUM_PT_LIGHTS 24
#endif

#define REVERSED_DEPTH

#ifndef TEX_COUNT
    #define TEX_COUNT 1
#endif

#if defined(NUM_PT_LIGHTS) && (NUM_PT_LIGHTS > 24)
    #define NUM_PT_LIGHTS 24
#endif

#include "includes/Terrain.hlsli"

struct VS_INPUT {
    float4 position : POSITION;
    float3 tangent : TANGENT;
    float3 binormal : BINORMAL;
    float3 normal : NORMAL;
    float4 uv : TEXCOORD0;
    float4 vertex_color : COLOR0;
    float4 blend_0 : TEXCOORD1;
    float4 blend_1 : TEXCOORD2;
};

struct VS_OUTPUT {
    float4 blend_0 : COLOR0;
    float4 blend_1 : COLOR1;
    float4 sPosition : POSITION;
    float2 uv : TEXCOORD0;
    float3 vertex_color : TEXCOORD1;
    float3 lPosition : TEXCOORD2;
    float3 tangent : TEXCOORD3;
    float3 binormal : TEXCOORD4;
    float3 normal : TEXCOORD5;
    float4 fogPosition : TEXCOORD6;
    float3 viewPosition : TEXCOORD7;
};

#ifdef VS

row_major float4x4 ModelViewProj : register(c0);

float4 FogParam : register(c14);
float3 FogColor : register(c15);
float4 EyePosition : register(c16);

VS_OUTPUT main(VS_INPUT IN) {
    VS_OUTPUT OUT;

    float4 posPS = mul(ModelViewProj, IN.position.xyzw);

    OUT.blend_0 = IN.blend_0;
    OUT.blend_1 = IN.blend_1;
    
    OUT.sPosition.xyzw = posPS;
    OUT.uv.xy = IN.uv.xy;
    OUT.vertex_color.xyz = clamp(IN.vertex_color.rgb, 0.0f, 1.0f);
    OUT.lPosition.xyz = IN.position.xyz;
    OUT.tangent.xyz = IN.tangent.xyz;
    OUT.binormal.xyz = IN.binormal.xyz;
    OUT.normal.xyz = IN.normal.xyz;
    OUT.fogPosition.xyzw = posPS;
    OUT.viewPosition.xyz = EyePosition.xyz;

    return OUT;
};

#endif  // Vertex shader.

struct PS_INPUT
{
    float2 uv : TEXCOORD0;
    float3 vertex_color : TEXCOORD1_centroid;
    float3 lPosition : TEXCOORD2_centroid;
    float3 tangent : TEXCOORD3_centroid;
    float3 binormal : TEXCOORD4_centroid;
    float3 normal : TEXCOORD5_centroid;
    float4 blend_0 : COLOR0;
    float4 blend_1 : COLOR1;
    float4 fogPosition : TEXCOORD6_centroid;
    float3 viewPosition : TEXCOORD7_centroid;
    float2 vpos : VPOS;
};

struct PS_OUTPUT {
    float4 color_0 : COLOR0;
    float4 color_1 : COLOR1;    // Field pass only (`GetFieldOutput`)
    float4 color_2 : COLOR2;
};

#ifdef PS

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
    PS_OUTPUT OUT;
    OUT.color_1 = 0.0f;
    OUT.color_2 = 0.0f;

    [branch] if (IsFieldPass()) {
        GetFieldOutput(IN.blend_0, IN.blend_1, IN.vertex_color, OUT.color_0, OUT.color_1, OUT.color_2);
        return OUT;
    }

    // In the overlay pass, shade the ground under the visible surface instead of this fragment (`GetGroundSample`).
    // Texture gradients are taken before the branch. Derivatives are not available inside dynamic flow control.
    float2 uv = IN.uv.xy;
    float3 lPosition = IN.lPosition.xyz;
    float4 blend0 = IN.blend_0;
    float4 blend1 = IN.blend_1;
    float2 dx = ddx(IN.uv.xy);
    float2 dy = ddy(IN.uv.xy);
    float overlayAlpha = 1.0f;
    float3 basisTangent = IN.tangent.xyz;
    float3 basisBinormal = IN.binormal.xyz;
    float3 basisNormal = IN.normal.xyz;
    [branch] if (IsOverlayPass()) {
        GroundSample ground = GetGroundSample(IN.vpos);
        overlayAlpha = ground.alpha;
        uv = ground.uv;
        dx = ground.uvDx;
        dy = ground.uvDy;
        lPosition = ground.position;
        blend0 = ground.blend0;
        blend1 = ground.blend1;
        GetGroundBasis(ground.groundNormal, basisTangent, basisBinormal, basisNormal);
    }

    int texCount = TEX_COUNT;
    float3 tangent = normalize(basisTangent);
    float3 binormal = normalize(basisBinormal);
    float3 normal = normalize(basisNormal);
    float3x3 tbn = float3x3(tangent, binormal, normal);
    float3 eyePosition = GetEyePosition(IN.viewPosition.xyz);
    float3 eyeDir = normalize(mul(tbn, eyePosition - lPosition));
    float dist = length(eyePosition - lPosition);
    
    float blends[7] = { blend0.x, blend0.y, blend0.z, blend0.w, blend1.x, blend1.y, blend1.z };
    float spec[7] = { LandSpec[0].x, LandSpec[0].y, LandSpec[0].z, LandSpec[0].w, LandSpec[1].x, LandSpec[1].y, LandSpec[1].z };

    float gloss = 0.0f;
    float specExponent = 0.0f;
    float3 baseColor = blendDiffuseMaps(GetVertexColor(IN.vertex_color, IN.vpos), uv, dx, dy, texCount, BaseMap, blends);
    float3 combinedNormal = blendNormalMaps(uv, dx, dy, texCount, NormalMap, blends, spec, gloss, specExponent);

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
    float3 fogPos = GetFogPosition(IN.fogPosition, lPosition);
    float fogStrength = 1 - saturate((FogParam.x - length(fogPos)) / FogParam.y);
    fog.rgb = FogColor.rgb;
    fog.a = pow(fogStrength, FogParam.z);
    
    float3 finalColor = lighting;
    finalColor = lerp(finalColor, fog.rgb, fog.a); // Apply fog.

    if (IsOverlayPass() && IsOverlayMaskDebug())
        finalColor = GetBlockColor(IBBlock.xy);

    OUT.color_0.a = overlayAlpha;
    OUT.color_0.rgb = finalColor;

    return OUT;
};

#endif  // Pixel shader.
