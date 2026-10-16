// Blend math for the terrain overlay pass.

// Height within which the visible surface touches the terrain and blends fully, in game units.
static const float SelfHeight = 0.5f;

// Lower limit for the sine of the surface tilt against the terrain. Keeps the distance to the contact line finite.
static const float MinTiltSine = 1e-3f;

// Lower limit for the blend range beyond `SelfHeight`. Keeps the fade edges of `smoothstep` apart.
static const float MinBlendRange = 1e-3f;

// Returns the fade of the terrain overlay towards the maximum distance, 1 near the camera and 0 at the maximum distance.
float GetDistanceFade(float sceneDepth, float maxDistance) {
    return 1.0f - smoothstep(maxDistance * 0.8f, maxDistance, sceneDepth);
}

// Returns the blend amount of the terrain overlay over a visible surface, 1 at the contact line and 0 at the end of the
// blend range. Without the hard edge, the amount is the opacity of the overlay.
//
// The height is the height of the visible surface above the terrain right under it, along the terrain normal.
// Both normals are normalized and in the same space. The surface normal can have either sign.
//
// The blend range is a distance along the surface, up from the contact line. For a surface tilted against the terrain
// by an angle, that distance is the height over the sine of the angle. Flat statics resting on the ground, such as
// slabs, lie their thickness above it and nearly parallel. Their distance to a contact line is far beyond the range.
//
// The ortho factor scales the blend range on faces at right angles to the terrain. Faces parallel to the terrain get
// the full range. In between, the range follows the cosine of the tilt.
float IntersectionBlendFactor(float height, float3 groundNormal, float3 surfaceNormal, float blendRange, float orthoFactor) {
    // Contact line. The overlay only draws on samples of tagged statics. This is never terrain over terrain.
    if (abs(height) < SelfHeight)
        return 1.0f;

    if (height < 0.0f)
        return 0.0f; // The visible surface lies below the terrain there.

    float tiltCosine = dot(surfaceNormal, groundNormal);
    float tiltSine = sqrt(saturate(1.0f - tiltCosine * tiltCosine));
    float contactDistance = height / max(tiltSine, MinTiltSine);
    float tiltRange = max(blendRange * lerp(orthoFactor, 1.0f, abs(tiltCosine)), SelfHeight + MinBlendRange);
    return 1.0f - smoothstep(SelfHeight, tiltRange, contactDistance);
}

// Size of a landscape block, a quarter of a cell. Blocks lie on a world-aligned grid.
// Each block draws with its own texture set and blend weights.
static const float LandscapeBlockSize = 2048.0f;

// Terrain texture coordinates are model space x and y over this, in every block. Block model space is cell-local.
// Textures wrap. World x and y over this give the same texels.
// Verified in RenderDoc capture 02.
static const float TerrainUVScale = 256.0f;

// Lower limit for 1 + cos of the surface tilt in the unrolled mapping. Caps the offset at ~2.6 times the height,
// for faces that overhang the ground.
static const float MinUnrollDenominator = 0.25f;

// Returns the unrolled mapping offset to add to the foot of the surface.
//
// On steep faces, every point of a column would sample the same spot under it. The texture would smear upwards.
// The offset continues past the contact line, under the static, as if the ground bent up onto it.
// That ground is hidden. Nothing shows twice and the texture flows on in the same direction.
// Takes the eye-relative surface position. The surface normal can have either sign.
//
// A surface point lies a distance d up the surface from the contact line. Its foot lies d * cos(tilt) from the contact
// line, on the static's side. The ground point lies d past the contact line, so the texture keeps its scale onto
// the surface. The offset from the foot is d * (1 - cos(tilt)), or the height * tan(tilt / 2). That is the height on
// vertical faces and goes to 0 on flat ones.
float3 GetUnrollOffset(float3 surface, float3 surfaceNormal, float3 groundNormal, float height) {
    float3 facingNormal = dot(surfaceNormal, surface) > 0.0f ? -surfaceNormal : surfaceNormal;
    float tiltCosine = dot(facingNormal, groundNormal);
    float3 outward = facingNormal - groundNormal * tiltCosine;

    // The outward part has the length sin(tilt), and tan(tilt / 2) = sin(tilt) / (1 + cos(tilt)).
    return -outward * max(height, 0.0f) / max(1.0f + tiltCosine, MinUnrollDenominator);
}

// Returns the shorter of the forward and backward difference to the neighbouring pixels.
// Across a silhouette, one of them jumps to another surface. The shorter one stays on this surface.
float3 GetShorterDifference(float3 center, float3 forward, float3 backward) {
    float3 forwardDifference = forward - center;
    float3 backwardDifference = center - backward;
    return dot(forwardDifference, forwardDifference) < dot(backwardDifference, backwardDifference) ? forwardDifference : backwardDifference;
}
