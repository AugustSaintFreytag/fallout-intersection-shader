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

float4 IBOverlayFlags : register(c89);  // x = OVERLAY_MAGIC during the replay, y = debug mode, z = max ray gap
float4 IBDepthParams : register(c90);   // x = near, y = far - near, z = near * far
float4 IBScreenParams : register(c91);  // x = 1 / width, y = 1 / height, z = blend range, w = max distance
sampler2D IBSceneDepth : register(s14);

bool IsOverlayPass() {
    return abs(IBOverlayFlags.x - OVERLAY_MAGIC) < 0.5f;
}

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
};

OverlayRay GetOverlayRay(float3 position, float3 eyePosition, float depth) {
    OverlayRay ray;
    ray.eyeToTerrain = position - eyePosition;
    ray.eyeToTerrainDx = ddx(ray.eyeToTerrain);
    ray.eyeToTerrainDy = ddy(ray.eyeToTerrain);
    ray.depth = depth;
    ray.depthDx = ddx(depth);
    ray.depthDy = ddy(depth);
    return ray;
}

float SampleSceneDepth(float2 uv) {
    return LinearizeDepth(tex2Dlod(IBSceneDepth, float4(uv, 0.0f, 0.0f)).r, IBDepthParams);
}

// Position of the visible surface on a view ray, relative to the eye. The ray points at the terrain plane at rayDepth.
float3 GetSurfacePosition(float3 ray, float rayDepth, float sceneDepth) {
    return ray * (sceneDepth / rayDepth);
}

// vpos: pixel position (VPOS).
float GetOverlayAlpha(float2 vpos, OverlayRay ray, float3 terrainNormal) {
    float2 texel = IBScreenParams.xy;
    float2 uv = (vpos + 0.5f) * texel;

    // The visible surface at this pixel and its right and lower neighbors give the surface normal.
    float sceneDepth = SampleSceneDepth(uv);
    float3 surface = GetSurfacePosition(ray.eyeToTerrain, ray.depth, sceneDepth);
    float3 surfaceX = GetSurfacePosition(ray.eyeToTerrain + ray.eyeToTerrainDx, ray.depth + ray.depthDx, SampleSceneDepth(uv + float2(texel.x, 0.0f)));
    float3 surfaceY = GetSurfacePosition(ray.eyeToTerrain + ray.eyeToTerrainDy, ray.depth + ray.depthDy, SampleSceneDepth(uv + float2(0.0f, texel.y)));
    float3 surfaceNormal = normalize(cross(surfaceX - surface, surfaceY - surface));

    float3 params = float3(IBScreenParams.z, IBScreenParams.w, IBOverlayFlags.z);
    return IntersectionBlendFactor(ray.depth, sceneDepth, ray.eyeToTerrain, normalize(terrainNormal), surfaceNormal, params);
}

static const float3 OverlayMaskColor = float3(1.0f, 0.0f, 1.0f);
