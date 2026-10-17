// Pixel shader for the feature replay of the topper blend.
//
// The plugin draws it in place of the object pixel shader while the topper passes draw a third time, after the colour
// replay. It writes the texture features at each topper pixel: positive on pebbles and other raised features, negative
// in the gaps between them. The composite shapes the edge of the topper blend with them, like the terrain overlay
// does with the ground texture (`GetEdgeFeatures`).
// The object shaders write the mesh texture coordinates to TEXCOORD0.xy and sample the base map at s0. Parallax
// Shaders – Objects binds the height map (`_p`) at s3.
// Verified in RenderDoc capture 02, EID 702.
//
// Reference at "Plugin/Source/DrawRedirect.cpp", "DrawWithFeatureShader".

sampler2D BaseMap : register(s0);
sampler2D HeightMap : register(s3);

float4 FeatureParams : register(c0); // x = 1 if the pass has a height map at s3

// Size the features stand out from, in texture coordinates. Pebbles in rubble textures span 1/32 to 1/8 of a repeat.
static const float FeatureSize = 1.0f / 16.0f;

// Gain on the features. Pebbles rise about 0.05-0.1 above the blurred height in common ground textures.
static const float FeatureGain = 8.0f;

// Rec. 709 luma weights.
static const float3 LumaWeights = float3(0.2126f, 0.7152f, 0.0722f);

// Returns the features at a texture coordinate, in [-1, 1].
//
// The height, or the luma of the base map without a height map, minus the same value blurred to the feature size.
// The blurred value comes from a coarser mip, picked by gradients of the feature size. Where the pixel footprint is
// larger than the feature size, both reads come from the same mip and the features fade out.
float4 main(float2 uv : TEXCOORD0) : COLOR0 {
    float2 dx = ddx(uv);
    float2 dy = ddy(uv);
    float2 blurDx = dx * max(1.0f, FeatureSize / max(length(dx), 1e-6f));
    float2 blurDy = dy * max(1.0f, FeatureSize / max(length(dy), 1e-6f));

    float features;
    [branch] if (FeatureParams.x > 0.5f) {
        features = tex2Dgrad(HeightMap, uv, dx, dy).r - tex2Dgrad(HeightMap, uv, blurDx, blurDy).r;
    }
    else {
        features = dot(tex2Dgrad(BaseMap, uv, dx, dy).rgb - tex2Dgrad(BaseMap, uv, blurDx, blurDy).rgb, LumaWeights);
    }

    return float4(clamp(features * FeatureGain, -1.0f, 1.0f) * 0.5f + 0.5f, 0.0f, 0.0f, 1.0f);
}
