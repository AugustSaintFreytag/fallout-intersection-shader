// Overlay mode for the terrain pixel shaders, shared by the VPT and Parallax variants.
//
// Intersection Blend replays the terrain passes after Depth Resolve's depth copy. During that replay it
// sets IBOverlayFlags.x to OVERLAY_MAGIC and binds the scene depth to s14. In the regular terrain pass the
// registers hold anything else, and the shader behaves exactly like the original.
//
// Registers: VPT and Parallax use c0-c88 and s0-s13, so ours start at c89 and s14.
// The plugin reads compiled shaders back and checks for the name "IBOverlayFlags" in the constant table
// to make sure these variants are installed, so keep that name.

#include "IntersectionBlend.hlsli"

static const float OVERLAY_MAGIC = 7919.0f;

float4 IBOverlayFlags : register(c89);  // x = OVERLAY_MAGIC during the replay, y = debug mode, z = max ray gap, w = lipping on
float4 IBDepthParams : register(c90);   // x = near, y = far - near, z = near * far
float4 IBScreenParams : register(c91);  // x = 1 / width, y = 1 / height, z = blend range, w = max distance
sampler2D IBSceneDepth : register(s14);

bool IsOverlayPass() {
    return abs(IBOverlayFlags.x - OVERLAY_MAGIC) < 0.5f;
}

// Debug modes 2 (overlay mask) and 3 (stencil tags, drawn on top of the mask).
bool IsOverlayMaskDebug() {
    return IBOverlayFlags.y > 1.5f;
}

// The view ray of the fragment, and its screen-space derivatives. Derivatives can't be taken inside the overlay
// branch (dynamic flow control), so the terrain shaders build this before branching.
struct OverlayRay {
    float3 eyeToTerrain;    // Eye to fragment, model space (terrain blocks are unrotated, so world orientation).
    float3 eyeToTerrainDx;
    float3 eyeToTerrainDy;
    float depth;            // Linear view depth of the fragment (clip-space w).
    float depthDx;
    float depthDy;
    float2 uvDx;            // Screen-space derivatives of the terrain texture coordinates.
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

// Where and how a terrain overlay fragment is shaded.
//
// The fragment itself is terrain behind the visible surface. Shading it as it is would paint the ground behind a
// static onto the static, which shifts with the view like a see-through hole. Instead it is shaded as the terrain
// right under the visible surface: the surface point projected onto the terrain plane along the terrain normal.
// That ground stays put in the world and meets the real terrain at the contact line, so it reads as terrain
// rising onto the static.
struct OverlaySample {
    float alpha;
    float2 uvOffset;        // Add to the terrain texture coordinates.
    float3 positionOffset;  // Add to the model space position, for the view direction and point lights.
    float distanceScale;    // Multiply view distances and texture gradients with this: the shaded point lies at the surface's depth.
};

float SampleSceneDepth(float2 uv) {
    return LinearizeDepth(tex2Dlod(IBSceneDepth, float4(uv, 0.0f, 0.0f)).r, IBDepthParams);
}

// Position of the visible surface on a view ray, relative to the eye. The ray points at the terrain plane at rayDepth.
float3 GetSurfacePosition(float3 ray, float rayDepth, float sceneDepth) {
    return ray * (sceneDepth / rayDepth);
}

// Texture coordinate change for a model space change in the terrain plane. Terrain texture coordinates are linear in
// model space x and y, so the screen-space derivatives of both give the mapping. Zero if they are degenerate.
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

// vpos: pixel position (VPOS).
OverlaySample GetOverlaySample(float2 vpos, OverlayRay ray, float3 terrainNormal) {
    float2 texel = IBScreenParams.xy;
    float2 uv = (vpos + 0.5f) * texel;
    terrainNormal = normalize(terrainNormal);

    // The visible surface at this pixel and its right and lower neighbors give the surface normal.
    float sceneDepth = SampleSceneDepth(uv);
    float3 surface = GetSurfacePosition(ray.eyeToTerrain, ray.depth, sceneDepth);
    float3 surfaceX = GetSurfacePosition(ray.eyeToTerrain + ray.eyeToTerrainDx, ray.depth + ray.depthDx, SampleSceneDepth(uv + float2(texel.x, 0.0f)));
    float3 surfaceY = GetSurfacePosition(ray.eyeToTerrain + ray.eyeToTerrainDy, ray.depth + ray.depthDy, SampleSceneDepth(uv + float2(0.0f, texel.y)));
    float3 surfaceNormal = normalize(cross(surfaceX - surface, surfaceY - surface));

    // Height of the visible surface above the terrain plane, independent of the view angle.
    float3 terrainToSurface = surface - ray.eyeToTerrain;
    float height = dot(terrainToSurface, terrainNormal);

    // The resolved depth holds one sample per pixel. Within this fragment's depth change across a pixel, it is this fragment.
    bool sameSurface = abs(sceneDepth - ray.depth) <= abs(ray.depthDx) + abs(ray.depthDy) + 0.05f;

    float3 params = float3(IBScreenParams.z, IBScreenParams.w, IBOverlayFlags.z);

    OverlaySample result;
    result.alpha = IntersectionBlendFactor(height, length(terrainToSurface), sceneDepth, sameSurface, terrainNormal, surfaceNormal, params);
    result.positionOffset = float3(0.0f, 0.0f, 0.0f);
    result.uvOffset = float2(0.0f, 0.0f);
    result.distanceScale = 1.0f;

    if (IBOverlayFlags.w > 0.5f) {
        result.positionOffset = terrainToSurface - terrainNormal * height;
        result.uvOffset = GetTerrainUVOffset(ray, result.positionOffset.xy);
        result.distanceScale = saturate(sceneDepth / ray.depth);
    }
    return result;
}

static const float3 OverlayMaskColor = float3(1.0f, 0.0f, 1.0f);
