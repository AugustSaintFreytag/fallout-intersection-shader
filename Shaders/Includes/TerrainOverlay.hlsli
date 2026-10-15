// Overlay and field modes for the terrain pixel shaders, shared by the VPT and Parallax variants.
//
// The plugin replays the terrain passes after Depth Resolve's depth copy. In that replay it sets `IBOverlayFlags.x`
// to `OVERLAY_MAGIC` and binds the ground buffers to s14 and s15. It draws a full-screen triangle in place of each
// landscape block's mesh. Every pixel is shaded by the block its ground point lies in (`GetGroundSample`).
// When the plugin draws the landscape from above for the height field, it sets `IBOverlayFlags.x` to `FIELD_MAGIC`
// instead. The shader then writes the attribute fields. In the regular terrain pass the registers hold any other
// value. The shader then behaves like the original.
//
// VPT and Parallax use c0-c88 and s0-s13. Overlay registers start at c89 and s14.
// The plugin checks compiled shaders for the name `IBOverlayFlags` in the constant table. Keep that name.

#include "IntersectionBlend.hlsli"
#include "Packing.hlsli"

static const float OVERLAY_MAGIC = 7919.0f;
static const float FIELD_MAGIC = 7907.0f;

float4 IBOverlayFlags : register(c89);  // x = OVERLAY_MAGIC during the replay, y = debug mode
float4 IBEyePosition : register(c90);   // xyz = camera position in world space
float4 IBScreenParams : register(c91);  // x = 1 / width, y = 1 / height
float4 IBBlock : register(c92);         // xy = grid index of the landscape block being drawn, in the replay and the field pass
float4 IBModelEye : register(c93);      // xyz = camera position in the block's model space
float4 IBFogRows[3] : register(c94);    // Eye-relative world position to the fog position (clip x, y, w - z)

// Written by the ground buffer pass: ground points (s14) and their packed attributes (s15).
//
// Reference at "Shaders/Ground/GroundBuffer.hlsl", "PS_OUTPUT".
sampler2D IBGroundBuffer : register(s14);
sampler2D IBGroundAttributes : register(s15);

bool IsOverlayPass() {
    return abs(IBOverlayFlags.x - OVERLAY_MAGIC) < 0.5f;
}

// Returns whether this is the height field pass. The shader then writes blend weights and vertex colour (`GetFieldOutput`).
bool IsFieldPass() {
    return abs(IBOverlayFlags.x - FIELD_MAGIC) < 0.5f;
}

// Returns whether debug mode 2 (overlay mask) or higher is active.
bool IsOverlayMaskDebug() {
    return IBOverlayFlags.y > 1.5f;
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

// Writes the field pass outputs for the three attribute fields: blend weights 0-3, blend weights 4-6 with the block
// code, vertex colour. The ground pass interpolates them at each ground point, within the block of the point.
void GetFieldOutput(float4 blend0, float4 blend1, float3 vertexColor, out float4 color0, out float4 color1, out float4 color2) {
    color0 = blend0;
    color1 = float4(blend1.xyz, GetBlockCode(IBBlock.xy));
    color2 = float4(vertexColor, 1.0f);
}

// Shading inputs of an overlay pixel.
//
// Nothing comes from the mesh. The plugin draws a full-screen triangle with zero interpolants in its place.
// Lighting happens in the model space of the block, like for the block's own pixels.
struct GroundSample {
    float alpha;
    float3 position;        // Ground point, model space
    float2 uv;
    float2 uvDx;            // Texture coordinate change to the neighbouring pixels, for mip selection
    float2 uvDy;
    float3 groundNormal;
    float4 blend0;
    float4 blend1;
};

float3 GetGroundPoint(float2 uv) {
    return tex2Dlod(IBGroundBuffer, float4(uv, 0.0f, 0.0f)).xyz;
}

// Returns the shading inputs at the given pixel position (VPOS).
// Discards pixels whose ground point lies in another block or that get no overlay.
GroundSample GetGroundSample(float2 vpos) {
    float2 texel = IBScreenParams.xy;
    float2 uv = (vpos + 0.5f) * texel;
    float4 points = tex2Dlod(IBGroundBuffer, float4(uv, 0.0f, 0.0f));
    float4 attributes = tex2Dlod(IBGroundAttributes, float4(uv, 0.0f, 0.0f));

    float3 blend012 = UnpackUnorm3x8(attributes.x);
    float3 blend345 = UnpackUnorm3x8(attributes.y);
    float3 blend6Color = UnpackUnorm3x8(attributes.z);
    float3 colorAlpha = UnpackUnorm3x8(attributes.w);

    GroundSample result;
    result.alpha = colorAlpha.y;
    float3 groundPoint = points.xyz;
    float2 block = floor((IBEyePosition.xy + groundPoint.xy) / LandscapeBlockSize);
    clip(result.alpha > 1.0f / 255.0f && all(block == IBBlock.xy) ? 1.0f : -1.0f);

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
    return result;
}

// The Parallax shaders with 5 to 7 textures use up to 28 of the 32 temporary registers of ps_3_0. The inputs that the
// overlay replaces are picked where they are used, from constants or by reading the ground buffers again. They are not
// held in registers through the shader. The mesh inputs they replace stay in input registers.

float3 GetEyePosition(float3 viewPosition) {
    return IsOverlayPass() ? IBModelEye.xyz : viewPosition;
}

float3 GetVertexColor(float3 vertexColor, float2 vpos) {
    [branch] if (IsOverlayPass()) {
        float4 attributes = tex2Dlod(IBGroundAttributes, float4((vpos + 0.5f) * IBScreenParams.xy, 0.0f, 0.0f));
        return float3(UnpackUnorm3x8(attributes.z).yz, UnpackUnorm3x8(attributes.w).x);
    }
    return vertexColor;
}

// Returns the fog position as the terrain shaders build it from the clip position: clip x, clip y, w - z with reversed depth.
// Takes the shaded model space position for the overlay.
float3 GetFogPosition(float4 clipPosition, float3 lPosition) {
    [branch] if (IsOverlayPass()) {
        float4 position = float4(lPosition - IBModelEye.xyz, 1.0f);
        return float3(dot(position, IBFogRows[0]), dot(position, IBFogRows[1]), dot(position, IBFogRows[2]));
    }

    float3 fogPosition = clipPosition.xyz;
    #ifdef REVERSED_DEPTH
        fogPosition.z = clipPosition.w - clipPosition.z;
    #endif
    return fogPosition;
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
