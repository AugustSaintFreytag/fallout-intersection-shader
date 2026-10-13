// Blend math for the terrain overlay pass.

#include "Depth.hlsli"

// Below this height (game units) the scene surface is the terrain itself, which the regular terrain pass already shows.
static const float SelfHeight = 0.5f;

// Surfaces tilted less than ~20 degrees against the terrain don't blend, more than ~45 degrees blend fully
// (values are 1 - cos(angle)). Keeps flat statics lying on the ground, like slabs and rubble, from being covered.
static const float FlatSurfaceStart = 0.06f;
static const float FlatSurfaceEnd = 0.29f;

// Opacity of a terrain overlay fragment lying behind the scene surface on the same view ray.
//
// terrainDepth/sceneDepth: linear view depths of the overlay fragment and of the visible surface (from the resolved depth).
// eyeToTerrain:            vector from the eye to the overlay fragment, in a space with world orientation.
// terrainNormal:           normalized terrain normal, same space.
// surfaceNormal:           normalized normal of the visible surface, same space, either sign.
// params:                  x = blend range, y = max distance, z = max ray gap.
float IntersectionBlendFactor(float terrainDepth, float sceneDepth, float3 eyeToTerrain, float3 terrainNormal, float3 surfaceNormal, float3 params) {
    float blendRange = params.x;
    float maxDistance = params.y;
    float maxRayGap = params.z;

    // Both depths lie on one view ray, so the visible surface sits at eyeToTerrain * ratio.
    float ratio = sceneDepth / terrainDepth;
    if (ratio > 1.001f)
        return 0.0f; // The terrain is in front of the resolved depth, the regular terrain pass already shows it.

    // Height of the visible surface above the terrain plane, independent of the view angle.
    float3 surfaceToTerrain = eyeToTerrain * (1.0f - ratio);
    float height = -dot(surfaceToTerrain, terrainNormal);
    if (height < SelfHeight)
        return 0.0f;

    float blend = 1.0f - smoothstep(SelfHeight, blendRange, height);

    // The plane test alone also accepts terrain far behind the surface, wherever the surface crosses that terrain's plane.
    blend *= 1.0f - smoothstep(maxRayGap * 0.5f, maxRayGap, length(surfaceToTerrain));

    blend *= smoothstep(FlatSurfaceStart, FlatSurfaceEnd, 1.0f - abs(dot(surfaceNormal, terrainNormal)));
    blend *= 1.0f - smoothstep(maxDistance * 0.8f, maxDistance, sceneDepth);
    return blend;
}
