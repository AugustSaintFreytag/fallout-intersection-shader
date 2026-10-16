// Ground mode shading for the terrain overlay, shared by the VPT and Parallax ground shaders.
//
// The plugin replays the landscape passes after Depth Resolve's depth copy. Each pass draws a full-screen triangle in
// place of its block's mesh, with a ground shader in place of the terrain pixel shader. The pass has bound the block's
// textures and constants. The ground shader shades every tagged pixel whose ground point lies in its block
// (`GetGroundSample`), from the ground buffers at s14 and s15.
//
// The terrain shaders of VPT and Parallax use c0-c88 and s0-s13. Overlay registers start at c89 and s14.

#include "IntersectionBlend.hlsli"
#include "Packing.hlsli"

float4 IBOverlayParams : register(c89); // x = debug mode, y = edge softness, z = edge feature strength
float4 IBEyePosition : register(c90);   // xyz = camera position in world space
float4 IBScreenParams : register(c91);  // x = 1 / width, y = 1 / height
float4 IBBlock : register(c92);         // xy = grid index of the landscape block being drawn
float4 IBModelEye : register(c93);      // xyz = camera position in the block's model space
float4 IBFogRows[3] : register(c94);    // Eye-relative world position to the fog position (clip x, y, w - z)
float4 IBEdgeParams : register(c97);    // x = edge feature size, y = blend range

// Written by the ground buffer pass: ground points (s14) and their packed attributes (s15).
//
// Reference at "Shaders/Ground/GroundBuffer.hlsl", "PS_OUTPUT".
sampler2D IBGroundBuffer : register(s14);
sampler2D IBGroundAttributes : register(s15);

// Returns whether debug mode 2 (overlay mask) or higher is active.
bool IsOverlayMaskDebug() {
    return IBOverlayParams.x > 1.5f;
}

// Returns whether debug mode 11 (edge features) is active.
bool IsEdgeFeatureDebug() {
    return abs(IBOverlayParams.x - 11.0f) < 0.5f;
}

// Turns a tangent frame onto the given ground normal, keeping its handedness.
void RebaseOnGround(inout float3 tangent, inout float3 binormal, inout float3 normal, float3 groundNormal) {
    float handedness = dot(cross(normal, tangent), binormal) < 0.0f ? -1.0f : 1.0f;
    normal = groundNormal;
    tangent = normalize(tangent - groundNormal * dot(tangent, groundNormal));
    binormal = cross(groundNormal, tangent) * handedness;
}

// Returns a hashed colour for a landscape block. Neighbouring blocks get different colours.
float3 GetBlockColor(float2 block) {
    float3 hash = frac(sin(float3(dot(block, float2(127.1f, 311.7f)), dot(block, float2(269.5f, 183.3f)), dot(block, float2(419.2f, 371.9f)))) * 43758.5453f);
    return 0.2f + 0.8f * hash;
}

// Shading inputs of an overlay pixel.
//
// Nothing comes from the mesh. Lighting happens in the model space of the block, like for the block's own pixels.
struct GroundSample {
    float amount;           // Blend amount, 1 at the contact line and 0 at the end of the blend range
    float distanceFade;
    float3 position;        // Ground point, model space
    float2 uv;
    float2 uvDx;            // Texture coordinate change to the neighbouring pixels, for mip selection
    float2 uvDy;
    float3 groundNormal;
    float4 blend0;
    float4 blend1;
    float3 vertexColor;
};

float3 GetGroundPoint(float2 uv) {
    return tex2Dlod(IBGroundBuffer, float4(uv, 0.0f, 0.0f)).xyz;
}

// Reads the shading inputs at the given pixel position (VPOS).
// Returns false for pixels that get no overlay or whose ground point lies in another block. The caller discards them
// with `clip` before any shading. Their stencil tag then stays for the pass of their own block.
bool GetGroundSample(float2 vpos, out GroundSample result) {
    result = (GroundSample)0;

    float2 texel = IBScreenParams.xy;
    float2 uv = (vpos + 0.5f) * texel;
    float4 points = tex2Dlod(IBGroundBuffer, float4(uv, 0.0f, 0.0f));
    float4 attributes = tex2Dlod(IBGroundAttributes, float4(uv, 0.0f, 0.0f));

    float3 colorAlpha = UnpackUnorm3x8(attributes.w);
    float3 groundPoint = points.xyz;
    float2 block = floor((IBEyePosition.xy + groundPoint.xy) / LandscapeBlockSize);
    [branch] if (colorAlpha.y <= 1.0f / 255.0f || colorAlpha.z <= 1.0f / 255.0f || any(block != IBBlock.xy))
        return false;

    float3 blend012 = UnpackUnorm3x8(attributes.x);
    float3 blend345 = UnpackUnorm3x8(attributes.y);
    float3 blend6Color = UnpackUnorm3x8(attributes.z);

    result.amount = colorAlpha.y;
    result.distanceFade = colorAlpha.z;

    float2 normal = UnpackUnorm2x12(points.w) * 2.0f - 1.0f;
    result.groundNormal = float3(normal, sqrt(saturate(1.0f - dot(normal, normal))));

    result.position = IBModelEye.xyz + groundPoint;
    result.uv = result.position.xy / TerrainUVScale;
    float3 groundDx = GetShorterDifference(groundPoint, GetGroundPoint(uv + float2(texel.x, 0.0f)), GetGroundPoint(uv - float2(texel.x, 0.0f)));
    float3 groundDy = GetShorterDifference(groundPoint, GetGroundPoint(uv + float2(0.0f, texel.y)), GetGroundPoint(uv - float2(0.0f, texel.y)));
    result.uvDx = groundDx.xy / TerrainUVScale;
    result.uvDy = groundDy.xy / TerrainUVScale;

    result.blend0 = float4(blend012, blend345.x);
    result.blend1 = float4(blend345.yz, blend6Color.x, 0.0f);
    result.vertexColor = float3(blend6Color.yz, colorAlpha.x);
    return true;
}

// Returns the fog position of a model space position, as the terrain shaders build it from the clip position:
// clip x, clip y, w - z with reversed depth.
float3 GetFogPosition(float3 position) {
    float4 eyeRelative = float4(position - IBModelEye.xyz, 1.0f);
    return float3(dot(eyeRelative, IBFogRows[0]), dot(eyeRelative, IBFogRows[1]), dot(eyeRelative, IBFogRows[2]));
}

// Returns the flat tangent frame turned onto the given ground normal.
// The terrain's own frame is +x, +y and up, tilted with the slope. The overlay has no mesh to take it from.
// Verified in RenderDoc capture 02.
void GetGroundBasis(float3 groundNormal, out float3 tangent, out float3 binormal, out float3 normal) {
    tangent = float3(1.0f, 0.0f, 0.0f);
    binormal = float3(0.0f, 1.0f, 0.0f);
    normal = float3(0.0f, 0.0f, 1.0f);
    RebaseOnGround(tangent, binormal, normal, groundNormal);
}

// Edge Shape

// Gain on the texture features. Pebbles rise about 0.05-0.1 above the blurred height in common ground textures.
static const float FeatureGain = 8.0f;

// Width of the antialiased edge, in blend amount per pixel footprint over the blend range.
// The blend amount falls by up to 1.5 per blend range, at the middle of its `smoothstep`.
static const float EdgeAntialiasSlope = 1.5f;
static const float MinEdgeWidth = 0.001f;

// Returns the texture features at a texture coordinate, in [-1, 1]: positive on pebbles and other raised features,
// negative in the gaps between them.
//
// The features are the height in the diffuse alpha minus the same height blurred to the edge feature size, weighted
// by the shading weights. Textures without a height add nothing (height status 0). The blurred height comes from a
// coarser mip, picked by gradients of the feature size. Where the pixel footprint is larger than the feature size,
// both heights come from the same mip and the features fade out.
float GetEdgeFeatures(float2 uv, float2 dx, float2 dy, int texCount, sampler2D tex[7], float weights[7], float status[7]) {
    float featureUV = IBEdgeParams.x / TerrainUVScale;
    float2 blurDx = dx * max(1.0f, featureUV / max(length(dx), 1e-6f));
    float2 blurDy = dy * max(1.0f, featureUV / max(length(dy), 1e-6f));

    float features = 0.0f;
    [unroll] for (int i = 0; i < texCount; i++) {
        if (status[i] > 0.0f)
            features += (tex2Dgrad(tex[i], uv, dx, dy).a - tex2Dgrad(tex[i], uv, blurDx, blurDy).a) * weights[i];
    }
    return clamp(features * FeatureGain, -1.0f, 1.0f);
}

// Returns the opacity of an overlay pixel.
//
// The texture features shift the blend amount: pebbles stay longer, the gaps between them go first. The shift is
// strongest halfway through the blend range and vanishes at both ends. The contact line stays covered, and nothing
// shows past the end of the range. The edge softness is the width of the fade around half the shifted amount. At 1 and
// without features, the opacity is the blend amount. Towards 0 the edge gets hard, antialiased over about a pixel.
float GetOverlayAlpha(GroundSample ground, float features) {
    float amount = ground.amount;
    float shaped = amount + 4.0f * amount * (1.0f - amount) * IBOverlayParams.z * features;

    float footprint = max(length(ground.uvDx), length(ground.uvDy)) * TerrainUVScale;
    float width = max(max(IBOverlayParams.y, footprint * EdgeAntialiasSlope / IBEdgeParams.y), MinEdgeWidth);
    return saturate((shaped - 0.5f) / width + 0.5f) * ground.distanceFade;
}
