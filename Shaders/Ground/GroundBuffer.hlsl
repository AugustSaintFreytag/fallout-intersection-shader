// Ground buffer: for every screen pixel, what the terrain overlay needs to know about the ground under the visible surface.
//
// Reads the scene depth (Depth Resolve's copy) and the terrain height field (the landscape drawn from straight above).
// Writes, as R32G32B32A32F:
//   r = linear view depth of the visible surface
//   g = height of that surface above the terrain under it, along the terrain normal (NO_GROUND where there is no terrain)
//   b, a = x and y of the terrain normal there (z is positive)
//
// The overlay reads this buffer from s14 instead of the scene depth, so the terrain shaders keep their sampler layout.

#include "../Includes/Depth.hlsli"

struct VS_INPUT {
    float3 position : POSITION;
    float2 uv : TEXCOORD0;
};

struct VS_OUTPUT {
    float4 position : POSITION;
    float2 uv : TEXCOORD0;
};

#ifdef VS

float4 TexelSize : register(c0);

VS_OUTPUT main(VS_INPUT IN) {
    VS_OUTPUT OUT;
    // D3D9 half-pixel offset so texels map onto pixels.
    OUT.position = float4(IN.position.xy + float2(-TexelSize.x, TexelSize.y), 0.0f, 1.0f);
    OUT.uv = IN.uv;
    return OUT;
}

#endif // VS

#ifdef PS

sampler2D SceneDepth : register(s0);
sampler2D HeightField : register(s1);

float4 DepthParams : register(c0);              // x = near, y = far - near, z = near * far
float4 ProjectionParams : register(c1);         // x = _11, y = _22, z = _31, w = _32 of the scene projection
float4 ViewToWorld[3] : register(c2);           // Rows: view space x, y and z axes in world orientation
row_major float4x4 WorldToField : register(c5); // Eye-relative world position to height field clip space
row_major float4x4 FieldToWorld : register(c9); // Height field clip space to eye-relative world position
float4 FieldParams : register(c13);             // x, y = height field texel size, z = maximum distance
float4 ScreenParams : register(c14);            // x, y = screen texel size

static const float NO_GROUND = 1.0e9f;

// D3D9 rasterizes at whole-number window coordinates: the texel or pixel at column i holds what lies at clip
// x = 2 i / width - 1, half a texel left of its centre, and likewise for rows.

// Eye-relative world position of the terrain stored in a height field texel.
// The height field is cleared to 0 (the far plane with reversed depth), so 0 means no terrain was drawn there.
float3 GetFieldPoint(float2 texel, inout bool valid) {
    float depth = tex2Dlod(HeightField, float4((texel + 0.5f) * FieldParams.xy, 0.0f, 0.0f)).r;
    valid = valid && depth > 0.0f;
    float2 position = texel * FieldParams.xy;
    float4 clip = float4(position.x * 2.0f - 1.0f, 1.0f - position.y * 2.0f, depth, 1.0f);
    float4 world = mul(clip, FieldToWorld);
    return world.xyz / world.w;
}

float4 main(VS_OUTPUT IN) : COLOR0 {
    float sceneDepth = LinearizeDepth(tex2Dlod(SceneDepth, float4(IN.uv, 0.0f, 0.0f)).r, DepthParams);

    // Eye-relative world position of the visible surface.
    float2 window = IN.uv - 0.5f * ScreenParams.xy;
    float2 ndc = float2(window.x * 2.0f - 1.0f, 1.0f - window.y * 2.0f);
    float3 viewPosition = float3((ndc.x - ProjectionParams.z) / ProjectionParams.x, (ndc.y - ProjectionParams.w) / ProjectionParams.y, 1.0f) * sceneDepth;
    float3 surface = viewPosition.x * ViewToWorld[0].xyz + viewPosition.y * ViewToWorld[1].xyz + viewPosition.z * ViewToWorld[2].xyz;

    float4 result = float4(sceneDepth, NO_GROUND, 0.0f, 0.0f);
    if (sceneDepth > FieldParams.z)
        return result;

    // The height field texel the surface lies over, and its neighbours for bilinear interpolation.
    float4 fieldClip = mul(float4(surface, 1.0f), WorldToField);
    float2 fieldUV = float2(fieldClip.x, -fieldClip.y) / fieldClip.w * 0.5f + 0.5f;
    if (any(fieldUV < 0.0f) || any(fieldUV > 1.0f))
        return result;

    float2 texel = fieldUV / FieldParams.xy;
    float2 base = floor(texel);
    float2 blend = texel - base;

    bool valid = true;
    float3 p00 = GetFieldPoint(base, valid);
    float3 p10 = GetFieldPoint(base + float2(1.0f, 0.0f), valid);
    float3 p01 = GetFieldPoint(base + float2(0.0f, 1.0f), valid);
    float3 p11 = GetFieldPoint(base + float2(1.0f, 1.0f), valid);
    if (!valid)
        return result;

    float groundZ = lerp(lerp(p00.z, p10.z, blend.x), lerp(p01.z, p11.z, blend.x), blend.y);
    float3 groundNormal = normalize(cross((p10 - p00) + (p11 - p01), (p01 - p00) + (p11 - p10)));
    groundNormal *= groundNormal.z < 0.0f ? -1.0f : 1.0f;

    result.g = (surface.z - groundZ) * groundNormal.z;
    result.ba = groundNormal.xy;
    return result;
}

#endif // PS
