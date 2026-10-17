// Full-screen pixel shaders for the topper blend, picked by define.
//
// COPY: copies the scene depth of Depth Resolve's first copy into an R32F target. Toppers are not in it yet.
// The composite reads it as the base depth after the scene depth has been resolved again with the toppers.
//
// COMPOSITE: runs on topper pixels in front of a tagged base (stencil bits 0x40 and 0x20), after the colour replay.
// It measures the height of the topper above the plane of the base, along the base normal. Over the static blend
// range above a floor, the base colour from before the toppers fades in over the topper's foot, with the contact
// shadow on the topper above it. The texture features of the topper shape the edge, like those of the ground texture
// in the terrain overlay. Both use the same edge (`GetEdgeCoverage`). Only floors blend.
// Debug mode 9 paints the coverage red. Debug mode 10 paints every candidate by why it blends or not (`GetCauseColor`).
// Debug mode 11 paints the features in grey over the candidates that blend.
//
// Reference at "Plugin/Source/TopperBlend.cpp".

#include "../Includes/Depth.hlsli"
#include "../Includes/IntersectionBlend.hlsli"

sampler2D SceneDepth : register(s0);    // INTZ, reversed depth
sampler2D BaseDepth : register(s1);     // R32F copy of the scene depth without toppers, COMPOSITE only
sampler2D BaseColor : register(s2);     // Copy of the scene colour without toppers, COMPOSITE only
sampler2D Features : register(s3);      // Texture features of the toppers, 0.5 + features / 2 in red, COMPOSITE only

float4 DepthParams : register(c0);      // x = near, y = far - near, z = near * far
float4 ProjectionParams : register(c1); // x = _11, y = _22, z = _31, w = _32 of the scene projection
float4 ViewToWorld[3] : register(c2);   // Rows: view space x, y and z axes in world orientation
float4 ScreenParams : register(c5);     // x, y = screen texel size
float4 EyePosition : register(c6);      // xyz = camera position in world space
float4 BlendParams : register(c7);      // x = static blend range, y = edge softness, z = debug mode 9 or 10, else 0,
                                        // w = contact shadow intensity
float4 ShadowParams : register(c8);     // x = contact shadow width, y = feature strength, z = 1 for debug mode 11

// Lowest z of the base normal that counts as floor. About 45 degrees of slope.
static const float FloorNormalZ = 0.7f;

// Returns the eye-relative world position of a reversed depth value at a pixel. Pixel i lies at clip x = 2 i / width - 1.
float3 GetPosition(float2 pixel, float depth) {
    float viewDepth = LinearizeDepth(depth, DepthParams);
    float2 window = pixel * ScreenParams.xy;
    float2 ndc = float2(window.x * 2.0f - 1.0f, 1.0f - window.y * 2.0f);
    float3 viewPosition = float3((ndc.x - ProjectionParams.z) / ProjectionParams.x, (ndc.y - ProjectionParams.w) / ProjectionParams.y, 1.0f) * viewDepth;
    return viewPosition.x * ViewToWorld[0].xyz + viewPosition.y * ViewToWorld[1].xyz + viewPosition.z * ViewToWorld[2].xyz;
}

float SampleDepth(sampler2D depthSampler, float2 pixel) {
    return tex2Dlod(depthSampler, float4((pixel + 0.5f) * ScreenParams.xy, 0.0f, 0.0f)).r;
}

float3 GetBasePosition(float2 pixel) {
    return GetPosition(pixel, SampleDepth(BaseDepth, pixel));
}

#ifdef COPY

float4 main(float2 vpos : VPOS) : COLOR0 {
    return SampleDepth(SceneDepth, vpos);
}

#endif // COPY

#ifdef COMPOSITE

// Returns the debug mode 10 colour of a candidate: red blends, yellow equal depths, blue not a floor, green floor
// above the static blend range.
float4 GetCauseColor(bool equalDepths, bool isFloor, bool blends) {
    if (blends)
        return float4(1.0f, 0.0f, 0.0f, 1.0f);
    if (equalDepths)
        return float4(1.0f, 1.0f, 0.0f, 1.0f);
    return isFloor ? float4(0.0f, 1.0f, 0.0f, 1.0f) : float4(0.0f, 0.0f, 1.0f, 1.0f);
}

// Height that stands for no topper in front of the base.
static const float NoTopperHeight = 1.0e9f;

// Returns the height of the topper at a pixel above a base plane, or `NoTopperHeight` where no topper lies in front of
// the base. Clears the flag if the pixel has no topper.
float GetTopperHeight(float2 pixel, float3 base, float3 normal, inout bool allToppers) {
    float baseDepth = SampleDepth(BaseDepth, pixel);
    float topperDepth = SampleDepth(SceneDepth, pixel);
    bool hasTopper = topperDepth != baseDepth;
    allToppers = allToppers && hasTopper;
    return hasTopper ? dot(GetPosition(pixel, topperDepth) - base, normal) : NoTopperHeight;
}

float4 main(float2 vpos : VPOS) : COLOR0 {
    float baseDepth = SampleDepth(BaseDepth, vpos);
    float topperDepth = SampleDepth(SceneDepth, vpos);
    bool equalDepths = topperDepth == baseDepth;

    float3 base = GetPosition(vpos, baseDepth);
    float3 topper = GetPosition(vpos, topperDepth);

    // Base normal from the neighbouring pixels on the same surface, facing the camera at the origin.
    float3 baseDx = GetShorterDifference(base, GetBasePosition(vpos + float2(1.0f, 0.0f)), GetBasePosition(vpos - float2(1.0f, 0.0f)));
    float3 baseDy = GetShorterDifference(base, GetBasePosition(vpos + float2(0.0f, 1.0f)), GetBasePosition(vpos - float2(0.0f, 1.0f)));
    float3 normal = normalize(cross(baseDx, baseDy));
    normal *= dot(normal, base) > 0.0f ? -1.0f : 1.0f;

    // Both depths are resolved from one sample. On the outline of a topper, that sample can lie on the base, and the
    // depths are equal. At grazing angles, the two depths of an outline pixel also come from spots that lie units apart
    // in depth. Outline pixels take the lowest height of their own and their topper neighbours. At the contact line the
    // neighbours inside the topper are low and the outline blends. At a silhouette high above the floor they are high
    // and the outline stays.
    float height = equalDepths ? NoTopperHeight : dot(topper - base, normal);
    bool allToppers = !equalDepths;
    float neighbourHeight = GetTopperHeight(vpos + float2(1.0f, 0.0f), base, normal, allToppers);
    neighbourHeight = min(neighbourHeight, GetTopperHeight(vpos - float2(1.0f, 0.0f), base, normal, allToppers));
    neighbourHeight = min(neighbourHeight, GetTopperHeight(vpos + float2(0.0f, 1.0f), base, normal, allToppers));
    neighbourHeight = min(neighbourHeight, GetTopperHeight(vpos - float2(0.0f, 1.0f), base, normal, allToppers));
    if (!allToppers)
        height = min(height, neighbourHeight);

    // The blend amount falls from 1 on the floor to 0 at the static blend range, like the terrain blend amount along
    // a surface. The features shift it with the envelope of the terrain overlay: strongest halfway, gone at both ends.
    // The sign is the opposite of the terrain overlay's. There the features belong to the material that covers, the
    // ground. Here they belong to the surface that gets covered, the topper. Its raised rocks stay and its gaps go
    // first. The edge is antialiased over the change of the amount to the neighbouring pixels.
    bool isFloor = normal.z >= FloorNormalZ;
    float amount = isFloor ? 1.0f - smoothstep(0.0f, BlendParams.x, height) : 0.0f;
    float2 uv = (vpos + 0.5f) * ScreenParams.xy;
    float features = tex2Dlod(Features, float4(uv, 0.0f, 0.0f)).r * 2.0f - 1.0f;
    float shaped = amount - 4.0f * amount * (1.0f - amount) * ShadowParams.y * features;
    float width = max(BlendParams.y, fwidth(shaped));
    float2 coverageShadow = GetEdgeCoverage(shaped, width, BlendParams.w, ShadowParams.x);

    if (BlendParams.z == 10.0f)
        return GetCauseColor(equalDepths, isFloor, amount > 0.0f);
    if (BlendParams.z == 9.0f)
        return float4(1.0f, 0.0f, 0.0f, coverageShadow.x);
    if (ShadowParams.z > 0.5f)
        return float4(features * 0.5f + 0.5f, features * 0.5f + 0.5f, features * 0.5f + 0.5f, amount > 0.0f ? 1.0f : 0.0f);

    float3 baseColor = tex2Dlod(BaseColor, float4(uv, 0.0f, 0.0f)).rgb;
    float4 output = ComposeEdge(baseColor, coverageShadow);
    clip(output.a > 0.0f ? 1.0f : -1.0f);
    return output;
}

#endif // COMPOSITE
