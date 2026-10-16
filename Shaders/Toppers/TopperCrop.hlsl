// Full-screen pixel shaders for the topper crop, picked by define.
//
// COPY: copies the scene depth of Depth Resolve's first copy into an R32F target. Toppers are not in it yet.
// The crop reads it as the base depth after the scene depth has been resolved again with the toppers.
//
// CROP: runs on pixels where a topper lies in front of a base (stencil bits 0x40 and 0x80). It measures the height of
// the topper above the plane of the base, along the base normal, and compares it with a noise threshold. Below the
// threshold, the pixel is cropped: it writes the base depth, and the stencil op clears the topper bit. The colour
// replay of the toppers then skips it. Only floors crop. Other pixels are discarded.
// In debug mode 10 no pixel is discarded. Each one shows why it is cropped or not (`GetCauseColor`).
//
// Reference at "Plugin/Source/TopperCrop.cpp".

#include "../Includes/Depth.hlsli"
#include "../Includes/IntersectionBlend.hlsli"

sampler2D SceneDepth : register(s0);    // INTZ, reversed depth
sampler2D BaseDepth : register(s1);     // R32F copy of the scene depth without toppers, CROP only

float4 DepthParams : register(c0);      // x = near, y = far - near, z = near * far
float4 ProjectionParams : register(c1); // x = _11, y = _22, z = _31, w = _32 of the scene projection
float4 ViewToWorld[3] : register(c2);   // Rows: view space x, y and z axes in world orientation
float4 ScreenParams : register(c5);     // x, y = screen texel size
float4 EyePosition : register(c6);      // xyz = camera position in world space
float4 CropParams : register(c7);       // x = crop height, y = edge grain size, z = 1 for debug mode 10,
                                        // w = edge noise amount

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

#ifdef CROP

struct PS_OUTPUT {
    float4 color : COLOR0;
    float depth : DEPTH;
};

// Returns the debug mode 10 colour of a crop candidate: red cropped, yellow equal depths, blue not a floor, green floor
// above the threshold.
float4 GetCauseColor(bool equalDepths, bool isFloor, bool cropped) {
    if (cropped)
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

PS_OUTPUT main(float2 vpos : VPOS) {
    float baseDepth = SampleDepth(BaseDepth, vpos);
    float topperDepth = SampleDepth(SceneDepth, vpos);
    bool debugCause = CropParams.z > 0.5f;
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
    // neighbours inside the topper are low and the outline is cropped. At a silhouette high above the floor they are
    // high and the outline stays.
    float height = equalDepths ? NoTopperHeight : dot(topper - base, normal);
    bool allToppers = !equalDepths;
    float neighbourHeight = GetTopperHeight(vpos + float2(1.0f, 0.0f), base, normal, allToppers);
    neighbourHeight = min(neighbourHeight, GetTopperHeight(vpos - float2(1.0f, 0.0f), base, normal, allToppers));
    neighbourHeight = min(neighbourHeight, GetTopperHeight(vpos + float2(0.0f, 1.0f), base, normal, allToppers));
    neighbourHeight = min(neighbourHeight, GetTopperHeight(vpos - float2(0.0f, 1.0f), base, normal, allToppers));
    if (!allToppers)
        height = min(height, neighbourHeight);

    // The same edge as the terrain overlay, with the blend amount 1 - height / crop height. Toppers have no material
    // height here. Their colour is drawn after the crop.
    float threshold = CropParams.x * GetEdgeReach(EyePosition.xy + topper.xy, 0.5f, 0.0f, CropParams.w, CropParams.y);
    bool isFloor = normal.z >= FloorNormalZ;
    bool cropped = isFloor && height < threshold;
    clip(cropped || debugCause ? 1.0f : -1.0f);

    // In debug mode 10 every candidate loses the topper bit. Pixels that are not cropped keep the topper depth.
    PS_OUTPUT OUT;
    OUT.color = debugCause ? GetCauseColor(equalDepths, isFloor, cropped) : float4(1.0f, 0.0f, 0.0f, 1.0f);
    OUT.depth = cropped ? baseDepth : topperDepth;
    return OUT;
}

#endif // CROP
