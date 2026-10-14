// Blend math for the terrain overlay pass.

// Height within which the visible surface touches the terrain and blends fully, in game units.
static const float SelfHeight = 0.5f;

// Surfaces tilted less than ~20 degrees against the terrain do not blend. Surfaces tilted more than ~45 degrees blend fully.
// Values are 1 - cos(angle). Keeps flat statics on the ground, such as slabs and rubble, from being covered.
static const float FlatSurfaceStart = 0.06f;
static const float FlatSurfaceEnd = 0.29f;

// Returns the opacity of a terrain overlay fragment drawn over a visible surface.
//
// The height is the height of the visible surface above the terrain right under it, along the terrain normal.
// It comes from the ground buffer.
//
// The ray gap is the distance between the visible surface and the terrain fragment along the view ray. The fragment
// supplies the texture blend weights. A fragment far from the surface would show the wrong ground, such as a cliff
// texture from behind a crest. The gap is a hard cutoff. The first fragment drawn on a pixel clears its stencil tag.
// A faint far fragment must not take the place of a near one.
//
// The same-surface flag is set if the resolved depth at this pixel is the terrain fragment itself. The overlay only
// draws on samples of tagged statics. Such a pixel is an MSAA edge pixel where a static meets the ground. It blends fully.
//
// Both normals are normalized and in the same space. The surface normal can have either sign.
// Parameters: x = blend range, y = max distance, z = max ray gap.
float IntersectionBlendFactor(float height, float rayGap, float sceneDepth, bool sameSurface, float3 groundNormal, float3 surfaceNormal, float3 params) {
    float blendRange = params.x;
    float maxDistance = params.y;
    float maxRayGap = params.z;

    if (rayGap > maxRayGap)
        return 0.0f;

    float distanceFade = 1.0f - smoothstep(maxDistance * 0.8f, maxDistance, sceneDepth);

    // Contact line. The overlay only draws on samples of tagged statics. This is never terrain over terrain.
    if (sameSurface || abs(height) < SelfHeight)
        return distanceFade;

    if (height < 0.0f)
        return 0.0f; // The visible surface lies below the terrain there.

    float blend = 1.0f - smoothstep(SelfHeight, blendRange, height);
    blend *= smoothstep(FlatSurfaceStart, FlatSurfaceEnd, 1.0f - abs(dot(surfaceNormal, groundNormal)));
    return blend * distanceFade;
}

// Size of a landscape block, a quarter of a cell. Blocks lie on a world-aligned grid.
// Each block draws with its own texture set and blend weights.
static const float LandscapeBlockSize = 2048.0f;

// Terrain texture coordinates are model space x and y over this, in every block. Block model space is cell-local.
// Textures wrap. World x and y over this give the same texels.
// Verified in RenderDoc capture 02.
static const float TerrainUVScale = 256.0f;

// Unrolled mapping fades in with the horizontal part of the surface normal, the sine of the surface tilt against the
// terrain. Flat surfaces have no direction to unroll in and sample straight down.
static const float UnrollStart = 0.1f;
static const float UnrollEnd = 0.3f;

// Returns the unrolled mapping offset to add to the foot of the surface.
//
// On steep faces, every point of a column would sample the same spot under it. The texture would smear upwards.
// The offset continues past the contact line by the height, under the static, as if the ground bent up onto it.
// That ground is hidden. Nothing shows twice and the texture flows on in the same direction.
// Takes the eye-relative surface position. The surface normal can have either sign.
float3 GetUnrollOffset(float3 surface, float3 surfaceNormal, float3 groundNormal, float height) {
    float3 facingNormal = dot(surfaceNormal, surface) > 0.0f ? -surfaceNormal : surfaceNormal;
    float3 outward = facingNormal - groundNormal * dot(facingNormal, groundNormal);
    float outwardLength = length(outward);
    float unroll = smoothstep(UnrollStart, UnrollEnd, outwardLength);
    return -outward / max(outwardLength, 1e-4f) * max(height, 0.0f) * unroll;
}

// Returns the shorter of the forward and backward difference to the neighbouring pixels.
// Across a silhouette, one of them jumps to another surface. The shorter one stays on this surface.
float3 GetShorterDifference(float3 center, float3 forward, float3 backward) {
    float3 forwardDifference = forward - center;
    float3 backwardDifference = center - backward;
    return dot(forwardDifference, forwardDifference) < dot(backwardDifference, backwardDifference) ? forwardDifference : backwardDifference;
}
