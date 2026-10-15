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

float4 IBOverlayParams : register(c89); // x = debug mode
float4 IBEyePosition : register(c90);   // xyz = camera position in world space
float4 IBScreenParams : register(c91);  // x = 1 / width, y = 1 / height
float4 IBBlock : register(c92);         // xy = grid index of the landscape block being drawn
float4 IBModelEye : register(c93);      // xyz = camera position in the block's model space
float4 IBFogRows[3] : register(c94);    // Eye-relative world position to the fog position (clip x, y, w - z)

// Written by the ground buffer pass: ground points (s14) and their packed attributes (s15).
//
// Reference at "Shaders/Ground/GroundBuffer.hlsl", "PS_OUTPUT".
sampler2D IBGroundBuffer : register(s14);
sampler2D IBGroundAttributes : register(s15);

// Returns whether debug mode 2 (overlay mask) or higher is active.
bool IsOverlayMaskDebug() {
    return IBOverlayParams.x > 1.5f;
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
    float alpha;
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
    [branch] if (colorAlpha.y <= 1.0f / 255.0f || any(block != IBBlock.xy))
        return false;

    float3 blend012 = UnpackUnorm3x8(attributes.x);
    float3 blend345 = UnpackUnorm3x8(attributes.y);
    float3 blend6Color = UnpackUnorm3x8(attributes.z);

    result.alpha = colorAlpha.y;

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
