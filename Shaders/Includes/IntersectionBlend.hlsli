// Blend math for the terrain overlay pass.

#include "Depth.hlsli"

// Within this height (game units) of the terrain plane, the visible surface touches the terrain: blend fully.
static const float SelfHeight = 0.5f;

// Surfaces tilted less than ~20 degrees against the terrain don't blend, more than ~45 degrees blend fully
// (values are 1 - cos(angle)). Keeps flat statics lying on the ground, like slabs and rubble, from being covered.
static const float FlatSurfaceStart = 0.06f;
static const float FlatSurfaceEnd = 0.29f;

// Opacity of a terrain overlay fragment lying behind the scene surface on the same view ray.
//
// height:          height of the visible surface above the terrain plane.
// rayGap:          distance between the visible surface and the terrain fragment along the view ray.
// sceneDepth:      linear view depth of the visible surface (from the resolved depth).
// sameSurface:     the resolved depth at this pixel is the terrain fragment itself. The overlay only draws on samples of
//                  tagged statics, so this is an MSAA edge pixel where a static meets the ground: blend fully.
// terrainNormal:   normalized terrain normal.
// surfaceNormal:   normalized normal of the visible surface, same space, either sign.
// params:          x = blend range, y = max distance, z = max ray gap.
float IntersectionBlendFactor(float height, float rayGap, float sceneDepth, bool sameSurface, float3 terrainNormal, float3 surfaceNormal, float3 params) {
    float blendRange = params.x;
    float maxDistance = params.y;
    float maxRayGap = params.z;

    float distanceFade = 1.0f - smoothstep(maxDistance * 0.8f, maxDistance, sceneDepth);

    // The plane test alone also accepts terrain far behind the surface, wherever the surface crosses that terrain's plane.
    float rayGapFade = 1.0f - smoothstep(maxRayGap * 0.5f, maxRayGap, rayGap);

    // At the contact line. The overlay only draws on samples of tagged statics, so this is never terrain over terrain.
    if (sameSurface || abs(height) < SelfHeight)
        return rayGapFade * distanceFade;

    if (height < 0.0f)
        return 0.0f; // The terrain is in front of the visible surface.

    float blend = 1.0f - smoothstep(SelfHeight, blendRange, height);
    blend *= smoothstep(FlatSurfaceStart, FlatSurfaceEnd, 1.0f - abs(dot(surfaceNormal, terrainNormal)));
    return blend * rayGapFade * distanceFade;
}
