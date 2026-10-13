// Fullscreen debug view of the depth Depth Resolve hands to the overlay pass.
// Tints the scene by linearized depth with a line every 128 units, fading out at fMaxDistance.

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

float4 main(VS_OUTPUT IN) : COLOR0 {
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
