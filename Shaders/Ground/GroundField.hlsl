// Pixel shader for the height field replay.
//
// The plugin draws it in place of the terrain pixel shader while the landscape passes draw into the height field.
// It writes the attribute fields, one per target: blend weights 0-3, blend weights 4-6 with the block code, vertex
// colour. The ground pass interpolates them at each ground point, within the block of the point.
// The inputs match the terrain vertex shader of Vanilla Plus Terrain and Parallax Shaders – Terrain.
//
// Reference at "Plugin/Source/GroundBuffer.cpp", "UpdateField".

#include "../Includes/Packing.hlsli"

float4 IBBlock : register(c92); // xy = grid index of the landscape block being drawn

struct PS_INPUT {
    float3 vertex_color : TEXCOORD1;
    float4 blend_0 : COLOR0;
    float4 blend_1 : COLOR1;
};

struct PS_OUTPUT {
    float4 color_0 : COLOR0;
    float4 color_1 : COLOR1;
    float4 color_2 : COLOR2;
};

PS_OUTPUT main(PS_INPUT IN) {
    PS_OUTPUT OUT;
    OUT.color_0 = IN.blend_0;
    OUT.color_1 = float4(IN.blend_1.xyz, GetBlockCode(IBBlock.xy));
    OUT.color_2 = float4(IN.vertex_color, 1.0f);
    return OUT;
}
