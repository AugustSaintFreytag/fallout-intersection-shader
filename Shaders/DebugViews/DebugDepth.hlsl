// Full-screen debug views, picked by the debug mode in `BlendParams.z`.
// 1: depth that Depth Resolve hands to the overlay pass. Tints the scene by linearized depth with a line every
//    128 units, fading out at `fMaxDistance`.
// 3: stencil tags. The plugin draws this view with a stencil test. Every drawn pixel is tagged.
// 4: ground buffer, bound to s0 in place of the depth. Shows the height of every surface above the terrain under it.
//    Green on the ground, a line every 8 units above it, red below it, blue where the height field has no terrain.

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

sampler2D DepthBuffer : register(s0);

float4 DepthParams : register(c0); // x = near, y = far - near, z = near * far
float4 BlendParams : register(c1); // x = max distance, y = blend range, z = debug mode

static const float BandSpacing = 128.0f;
static const float DebugModeStencilTags = 3.0f;
static const float DebugModeGroundHeight = 4.0f;
static const float GroundBandSpacing = 8.0f;

float4 GroundHeightView(float4 ground, float maxDistance) {
    if (ground.r > maxDistance)
        return float4(0.0f, 0.0f, 0.0f, 0.0f);
    if (ground.g > 1.0e8f)
        return float4(0.0f, 0.2f, 1.0f, 0.4f);

    float height = ground.g;
    if (abs(height) < 1.0f)
        return float4(0.0f, 1.0f, 0.0f, 0.3f);
    if (height < 0.0f)
        return float4(1.0f, 0.0f, 0.0f, 0.5f);

    float bandLine = step(0.85f, frac(height / GroundBandSpacing));
    float fade = 1.0f - saturate(height / (GroundBandSpacing * 8.0f));
    return float4(1.0f, 0.9f, 0.2f, lerp(0.15f, 0.9f, bandLine) * fade);
}

float4 main(VS_OUTPUT IN) : COLOR0 {
    if (BlendParams.z == DebugModeStencilTags)
        return float4(0.0f, 1.0f, 1.0f, 0.5f);
    if (BlendParams.z == DebugModeGroundHeight)
        return GroundHeightView(tex2D(DepthBuffer, IN.uv), BlendParams.x);

    float depth = LinearizeDepth(tex2D(DepthBuffer, IN.uv).r, DepthParams);
    float maxDistance = BlendParams.x;

    float distanceFraction = saturate(depth / maxDistance);
    float3 color = lerp(float3(0.0f, 0.4f, 1.0f), float3(1.0f, 0.4f, 0.0f), distanceFraction);

    float band = frac(depth / BandSpacing);
    float bandLine = step(0.95f, band);

    float alpha = depth < maxDistance ? lerp(0.25f, 0.8f, bandLine) : 0.0f;
    return float4(lerp(color, float3(1.0f, 1.0f, 1.0f), bandLine), alpha);
}

#endif // PS
