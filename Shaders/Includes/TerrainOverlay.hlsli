// Overlay mode for the terrain pixel shaders, shared by the VPT and Parallax variants.
//
// The plugin replays the terrain passes after Depth Resolve's depth copy. In that replay it sets `IBOverlayFlags.x`
// to `OVERLAY_MAGIC` and binds the ground buffers to s14 and s15. When it draws the landscape from above for the
// height field, it sets `IBOverlayFlags.x` to `FIELD_MAGIC` instead. The shader then writes the attribute fields.
// In the regular terrain pass the registers hold any other value. The shader then behaves like the original.
//
// VPT and Parallax use c0-c88 and s0-s13. Overlay registers start at c89 and s14.
// The plugin checks compiled shaders for the name `IBOverlayFlags` in the constant table. Keep that name.

#include "IntersectionBlend.hlsli"
#include "Packing.hlsli"

static const float OVERLAY_MAGIC = 7919.0f;
static const float FIELD_MAGIC = 7907.0f;

float4 IBOverlayFlags : register(c89);  // x = OVERLAY_MAGIC during the replay, y = debug mode, z = max ray gap,
                                        // w = lipping mode (0 off, 1 straight down, 2 unrolled, 3 ground mode)
float4 IBEyePosition : register(c90);   // xyz = camera position in world space
float4 IBScreenParams : register(c91);  // x = 1 / width, y = 1 / height, z = blend range, w = max distance
float4 IBBlock : register(c92);         // Ground mode and field pass: xy = grid index of the landscape block being drawn
float4 IBModelEye : register(c93);      // Ground mode: xyz = camera position in the block's model space
float4 IBFogRows[3] : register(c94);    // Ground mode: eye-relative world position to the fog position (clip x, y, w - z)

// Written by the ground buffer pass.
// Lipping modes 0-2: r = linear depth, g = height above the terrain under the surface, b, a = terrain normal x and y.
// Ground mode: ground points (s14) and their packed attributes (s15).
//
// Reference at "Shaders/Ground/GroundBuffer.hlsl", "PS_OUTPUT".
sampler2D IBGroundBuffer : register(s14);
sampler2D IBGroundAttributes : register(s15);

// The ground buffer height is far above this where no terrain lies under the surface.
static const float NoGroundHeight = 1.0e8f;

bool IsOverlayPass() {
    return abs(IBOverlayFlags.x - OVERLAY_MAGIC) < 0.5f;
}

// Returns whether this is the height field pass. The shader then writes blend weights and vertex colour (`GetFieldOutput`).
bool IsFieldPass() {
    return abs(IBOverlayFlags.x - FIELD_MAGIC) < 0.5f;
}

// Returns whether the overlay runs in ground mode. The plugin then draws a full-screen triangle in place of each
// landscape block's mesh. Every pixel is shaded by the block its ground point lies in (`GetGroundSample`).
bool IsGroundMode() {
    return IBOverlayFlags.w > 2.5f;
}

// Returns whether debug mode 2 (overlay mask) or higher is active.
bool IsOverlayMaskDebug() {
    return IBOverlayFlags.y > 1.5f;
}

// The view ray of a fragment with its screen-space derivatives.
// Derivatives are not available inside the overlay branch, which uses dynamic flow control. The terrain shaders build
// this before branching.
struct OverlayRay {
    float3 eyeToTerrain;    // Eye to fragment, model space. Blocks are unrotated.
    float3 eyeToTerrainDx;
    float3 eyeToTerrainDy;
    float depth;            // Linear view depth of the fragment, clip-space w
    float depthDx;
    float depthDy;
    float2 uvDx;            // Screen-space derivatives of the terrain texture coordinates
    float2 uvDy;
};

OverlayRay GetOverlayRay(float3 position, float3 eyePosition, float depth, float2 uv) {
    OverlayRay ray;
    ray.eyeToTerrain = position - eyePosition;
    ray.eyeToTerrainDx = ddx(ray.eyeToTerrain);
    ray.eyeToTerrainDy = ddy(ray.eyeToTerrain);
    ray.depth = depth;
    ray.depthDx = ddx(depth);
    ray.depthDy = ddy(depth);
    ray.uvDx = ddx(uv);
    ray.uvDy = ddy(uv);
    return ray;
}

// Opacity and shading offsets of a terrain overlay fragment.
//
// The fragment is terrain behind the visible surface. It decides where the overlay can draw. The height and normal
// of the ground come from the ground buffer, which holds the terrain right under the visible surface. Shading the
// fragment as is would paint the ground behind a static onto the static. That ground shifts with the view like a
// see-through hole. The fragment is shaded as the terrain under the visible surface instead. That ground stays in
// place and meets the real terrain at the contact line. It reads as terrain rising onto the static.
//
// The fragment still supplies its texture set, blend weights and vertex colour. A fragment only draws if it lies on
// the same ground as the point it shades (`IsSameGround`). Otherwise terrain behind a crest or cliff would paint its
// own textures onto the static.
struct OverlaySample {
    float alpha;
    float2 uvOffset;        // Add to the terrain texture coordinates
    float3 positionOffset;  // Add to the model space position, for the view direction and point lights
    float distanceScale;    // Factor on view distances and texture gradients. The shaded point lies at the depth of the surface.
    float3 groundNormal;    // Ground normal under the visible surface, for lighting
};

// Returns whether the fragment can shade the ground at its position plus the given offset.
// For terrain, both have to lie in the same landscape block. Blending onto other statics will need its own test here, by object.
bool IsSameGround(OverlayRay ray, float3 groundOffset) {
    float2 fragment = IBEyePosition.xy + ray.eyeToTerrain.xy;
    return all(floor(fragment / LandscapeBlockSize) == floor((fragment + groundOffset.xy) / LandscapeBlockSize));
}

// Turns the fragment's tangent frame onto the given ground normal, keeping its handedness.
// Behind a crest the fragment lies on the far slope. Its own normal would light the blend as if it faced that slope.
void RebaseOnGround(inout float3 tangent, inout float3 binormal, inout float3 normal, float3 groundNormal) {
    float handedness = dot(cross(normal, tangent), binormal) < 0.0f ? -1.0f : 1.0f;
    normal = groundNormal;
    tangent = normalize(tangent - groundNormal * dot(tangent, groundNormal));
    binormal = cross(groundNormal, tangent) * handedness;
}

float4 SampleGround(float2 uv) {
    return tex2Dlod(IBGroundBuffer, float4(uv, 0.0f, 0.0f));
}

// Returns the eye-relative position of the visible surface on a view ray.
// The ray points at the terrain fragment at the given ray depth.
float3 GetSurfacePosition(float3 ray, float rayDepth, float sceneDepth) {
    return ray * (sceneDepth / rayDepth);
}

// Returns the texture coordinate change for a model space change in the terrain plane.
// Terrain texture coordinates are linear in model space x and y. The screen-space derivatives of both give the mapping.
// Returns zero if the derivatives are degenerate.
float2 GetTerrainUVOffset(OverlayRay ray, float2 positionOffset) {
    float2x2 positionD = float2x2(ray.eyeToTerrainDx.xy, ray.eyeToTerrainDy.xy);
    float2x2 uvD = float2x2(ray.uvDx, ray.uvDy);

    // As rows: uvD = positionD * M, where M maps a model space row vector to texture coordinates.
    float det = positionD._11 * positionD._22 - positionD._12 * positionD._21;
    float scale = dot(positionD[0], positionD[0]) + dot(positionD[1], positionD[1]);
    if (abs(det) <= 1e-4f * scale)
        return float2(0.0f, 0.0f);

    float2x2 positionDInverse = float2x2(positionD._22, -positionD._12, -positionD._21, positionD._11) / det;
    return mul(positionOffset, mul(positionDInverse, uvD));
}

// Returns the opacity and shading offsets of the overlay fragment at the given pixel position (VPOS).
OverlaySample GetOverlaySample(float2 vpos, OverlayRay ray) {
    float2 texel = IBScreenParams.xy;
    float2 uv = (vpos + 0.5f) * texel;

    OverlaySample result;
    result.alpha = 0.0f;
    result.positionOffset = float3(0.0f, 0.0f, 0.0f);
    result.uvOffset = float2(0.0f, 0.0f);
    result.distanceScale = 1.0f;
    result.groundNormal = float3(0.0f, 0.0f, 1.0f);

    float4 ground = SampleGround(uv);
    if (ground.g > NoGroundHeight)
        return result;

    float sceneDepth = ground.r;
    float height = ground.g;
    float3 groundNormal = float3(ground.b, ground.a, sqrt(saturate(1.0f - dot(ground.ba, ground.ba))));
    result.groundNormal = groundNormal;

    // The visible surface at this pixel and its right and lower neighbors give the surface normal.
    float3 surface = GetSurfacePosition(ray.eyeToTerrain, ray.depth, sceneDepth);
    float3 surfaceX = GetSurfacePosition(ray.eyeToTerrain + ray.eyeToTerrainDx, ray.depth + ray.depthDx, SampleGround(uv + float2(texel.x, 0.0f)).r);
    float3 surfaceY = GetSurfacePosition(ray.eyeToTerrain + ray.eyeToTerrainDy, ray.depth + ray.depthDy, SampleGround(uv + float2(0.0f, texel.y)).r);
    float3 surfaceNormal = normalize(cross(surfaceX - surface, surfaceY - surface));

    float3 terrainToSurface = surface - ray.eyeToTerrain;

    // The resolved depth holds one sample per pixel. Within the depth change of this fragment across a pixel, the sample is this fragment.
    bool sameSurface = abs(sceneDepth - ray.depth) <= abs(ray.depthDx) + abs(ray.depthDy) + 0.05f;

    float3 params = float3(IBScreenParams.z, IBScreenParams.w, IBOverlayFlags.z);
    result.alpha = IntersectionBlendFactor(height, length(terrainToSurface), sceneDepth, sameSurface, groundNormal, surfaceNormal, params);

    float lippingMode = IBOverlayFlags.w;
    if (lippingMode > 0.5f) {
        // Offset to the terrain right under the surface point.
        float3 offset = terrainToSurface - groundNormal * height;

        if (lippingMode > 1.5f)
            offset += GetUnrollOffset(surface, surfaceNormal, groundNormal, height);

        if (!IsSameGround(ray, offset))
            result.alpha = 0.0f;

        result.positionOffset = offset;
        result.uvOffset = GetTerrainUVOffset(ray, offset.xy);
        result.distanceScale = saturate(sceneDepth / ray.depth);
    }
    return result;
}

static const float3 OverlayMaskColor = float3(1.0f, 0.0f, 1.0f);

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

// Shading inputs of a pixel in ground mode.
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

// Returns the ground mode shading inputs at the given pixel position (VPOS).
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

// The 6- and 7-texture Parallax shaders come close to the 32 temporary registers of ps_3_0. The inputs that ground
// mode replaces are picked where they are used, from constants or by reading the ground buffers again. They are not
// held in registers through the shader. The mesh inputs they replace stay in input registers.

float3 GetEyePosition(float3 viewPosition) {
    return IsOverlayPass() && IsGroundMode() ? IBModelEye.xyz : viewPosition;
}

float3 GetVertexColor(float3 vertexColor, float2 vpos) {
    [branch] if (IsOverlayPass() && IsGroundMode()) {
        float4 attributes = tex2Dlod(IBGroundAttributes, float4((vpos + 0.5f) * IBScreenParams.xy, 0.0f, 0.0f));
        return float3(UnpackUnorm3x8(attributes.z).yz, UnpackUnorm3x8(attributes.w).x);
    }
    return vertexColor;
}

// Returns the fog position as the terrain shaders build it from the clip position: clip x, clip y, w - z with reversed depth.
// Takes the shaded model space position for ground mode and the distance scale of `OverlaySample` for lipping modes 0-2.
float3 GetFogPosition(float4 clipPosition, float3 lPosition, float distanceScale) {
    [branch] if (IsOverlayPass() && IsGroundMode()) {
        float4 position = float4(lPosition - IBModelEye.xyz, 1.0f);
        return float3(dot(position, IBFogRows[0]), dot(position, IBFogRows[1]), dot(position, IBFogRows[2]));
    }

    float3 fogPosition = clipPosition.xyz;
    #ifdef REVERSED_DEPTH
        fogPosition.z = clipPosition.w - clipPosition.z;
    #endif
    return fogPosition * distanceScale;
}

// Returns the flat tangent frame turned onto the given ground normal.
// The terrain's own frame is +x, +y and up, tilted with the slope. Ground mode has no mesh to take it from.
// Verified in RenderDoc capture 02.
void GetGroundBasis(float3 groundNormal, out float3 tangent, out float3 binormal, out float3 normal) {
    tangent = float3(1.0f, 0.0f, 0.0f);
    binormal = float3(0.0f, 1.0f, 0.0f);
    normal = float3(0.0f, 0.0f, 1.0f);
    RebaseOnGround(tangent, binormal, normal, groundNormal);
}

float3 GetOverlayMaskColor() {
    return IsGroundMode() ? GetBlockColor(IBBlock.xy) : OverlayMaskColor;
}
