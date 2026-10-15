// Ground buffer: for every screen pixel, what the terrain overlay needs to know about the ground under the visible surface.
//
// Reads the scene depth copy of Depth Resolve, the height field and the attribute fields.
// Writes two R32G32B32A32F targets, bound to s14 and s15 in the terrain shaders (`PS_OUTPUT`).
//
// The overlay reads these buffers instead of the scene depth. The terrain shaders keep their sampler layout.

#include "../Includes/Depth.hlsli"
#include "../Includes/IntersectionBlend.hlsli"
#include "../Includes/Packing.hlsli"

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
// Attribute fields, texel for texel with the height field
sampler2D FieldBlend0 : register(s2);   // Blend weights 0-3
sampler2D FieldBlend1 : register(s3);   // Blend weights 4-6, a = block code
sampler2D FieldColor : register(s4);    // Vertex colour

float4 DepthParams : register(c0);              // x = near, y = far - near, z = near * far
float4 ProjectionParams : register(c1);         // x = _11, y = _22, z = _31, w = _32 of the scene projection
float4 ViewToWorld[3] : register(c2);           // Rows: view space x, y and z axes in world orientation
row_major float4x4 WorldToField : register(c5); // Eye-relative world position to height field clip space
row_major float4x4 FieldToWorld : register(c9); // Height field clip space to eye-relative world position
float4 FieldParams : register(c13);             // x, y = height field texel size, z = maximum distance
float4 ScreenParams : register(c14);            // x, y = screen texel size
float4 GroundParams : register(c15);            // x = 1 for the height layout (debug mode 4), y = blend range,
                                                // z = 1 for unrolled mapping, w = 1 for the raw attributes (debug mode 7)
float4 EyePosition : register(c16);             // xyz = camera position in world space

static const float NO_GROUND = 1.0e9f;

// D3D9 rasterizes at whole-number window coordinates. The texel or pixel at column i holds what lies at
// clip x = 2 i / width - 1, half a texel left of its centre. The same applies to rows.

// Returns the eye-relative world position of the terrain in a height field texel.
// Clears the valid flag if no terrain was drawn there. The height field is cleared to 0, the far plane with reversed depth.
float3 GetFieldPoint(float2 texel, inout bool valid) {
    float depth = tex2Dlod(HeightField, float4((texel + 0.5f) * FieldParams.xy, 0.0f, 0.0f)).r;
    valid = valid && depth > 0.0f;
    float2 position = texel * FieldParams.xy;
    float4 clip = float4(position.x * 2.0f - 1.0f, 1.0f - position.y * 2.0f, depth, 1.0f);
    float4 world = mul(clip, FieldToWorld);
    return world.xyz / world.w;
}

// Returns the height field texel coordinates of an eye-relative world position. Texel i lies at i.
float2 GetFieldTexel(float3 position) {
    float4 fieldClip = mul(float4(position, 1.0f), WorldToField);
    return (float2(fieldClip.x, -fieldClip.y) / fieldClip.w * 0.5f + 0.5f) / FieldParams.xy;
}

// Returns the eye-relative world position of the visible surface at a screen texture coordinate, with its linear depth.
float3 GetSurface(float2 uv, out float sceneDepth) {
    sceneDepth = LinearizeDepth(tex2Dlod(SceneDepth, float4(uv, 0.0f, 0.0f)).r, DepthParams);
    float2 window = uv - 0.5f * ScreenParams.xy;
    float2 ndc = float2(window.x * 2.0f - 1.0f, 1.0f - window.y * 2.0f);
    float3 viewPosition = float3((ndc.x - ProjectionParams.z) / ProjectionParams.x, (ndc.y - ProjectionParams.w) / ProjectionParams.y, 1.0f) * sceneDepth;
    return viewPosition.x * ViewToWorld[0].xyz + viewPosition.y * ViewToWorld[1].xyz + viewPosition.z * ViewToWorld[2].xyz;
}

float3 GetSurface(float2 uv) {
    float sceneDepth;
    return GetSurface(uv, sceneDepth);
}

struct FieldAttributes {
    float4 blend0;
    float3 blend1;
    float3 vertexColor;
};

// Returns blend weights and vertex colour at an eye-relative ground point.
// Interpolates bilinearly between the texels of the point's own landscape block. Texels of a neighbouring block weigh
// other textures and are left out.
FieldAttributes GetFieldAttributes(float3 groundPoint) {
    float2 texel = GetFieldTexel(groundPoint);
    float2 base = floor(texel);
    float2 blend = texel - base;
    float code = GetBlockCode(floor((EyePosition.xy + groundPoint.xy) / LandscapeBlockSize));

    FieldAttributes result;
    result.blend0 = 0.0f;
    result.blend1 = 0.0f;
    result.vertexColor = 0.0f;
    float totalWeight = 0.0f;

    // Nearest texel to the point. Used if no texel of the bilinear footprint lies in its block.
    float2 nearest = floor(texel + 0.5f);
    FieldAttributes fallback = result;

    [unroll] for (int i = 0; i < 4; i++) {
        float2 offset = float2(i % 2, i / 2);
        float4 uv = float4((base + offset + 0.5f) * FieldParams.xy, 0.0f, 0.0f);
        float4 blend0 = tex2Dlod(FieldBlend0, uv);
        float4 blend1 = tex2Dlod(FieldBlend1, uv);
        float3 vertexColor = tex2Dlod(FieldColor, uv).rgb;

        float2 axisWeights = lerp(1.0f - blend, blend, offset);
        float weight = axisWeights.x * axisWeights.y * (IsSameBlockCode(blend1.a, code) ? 1.0f : 0.0f);
        result.blend0 += blend0 * weight;
        result.blend1 += blend1.rgb * weight;
        result.vertexColor += vertexColor * weight;
        totalWeight += weight;

        if (all(base + offset == nearest)) {
            fallback.blend0 = blend0;
            fallback.blend1 = blend1.rgb;
            fallback.vertexColor = vertexColor;
        }
    }

    if (totalWeight < 1e-3f)
        return fallback;

    result.blend0 /= totalWeight;
    result.blend1 /= totalWeight;
    result.vertexColor /= totalWeight;
    return result;
}

// Ground buffer targets. Pixels without terrain under them or beyond the maximum distance get zero in both targets.
// The overlay shades every pixel from these targets alone (`GetGroundSample`):
//   COLOR0 = xyz: eye-relative ground point that shades the surface (its foot, unrolled if enabled),
//            w: terrain normal x and y, as 2 x 12 bits of x * 0.5 + 0.5
//   COLOR1 = 3 x 8 bits per channel: x = blend weights 0-2, y = blend weights 3-5, z = blend weight 6 and vertex
//            colour r, g, w = vertex colour b, overlay opacity, 0
//
// Height layout, for debug mode 4 only. The overlay does not run with it. Pixels without terrain under them get
// `NO_GROUND` in COLOR0.g:
//   COLOR0 = linear view depth of the visible surface, height of the surface above the terrain under it along the
//            terrain normal, terrain normal x and y (z is positive)
//   COLOR1 = 0
struct PS_OUTPUT {
    float4 target0 : COLOR0;
    float4 target1 : COLOR1;
};

PS_OUTPUT main(VS_OUTPUT IN) {
    bool heightLayout = GroundParams.x > 0.5f;

    float sceneDepth;
    float3 surface = GetSurface(IN.uv, sceneDepth);

    PS_OUTPUT result;
    result.target0 = heightLayout ? float4(sceneDepth, NO_GROUND, 0.0f, 0.0f) : 0.0f;
    result.target1 = 0.0f;
    if (sceneDepth > FieldParams.z)
        return result;

    // Height field texel under the surface and its neighbours for bilinear interpolation.
    float2 texel = GetFieldTexel(surface);
    if (any(texel < 0.0f) || any(texel * FieldParams.xy > 1.0f))
        return result;

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

    float height = (surface.z - groundZ) * groundNormal.z;
    float3 foot = surface - groundNormal * height;

    if (heightLayout) {
        result.target0 = float4(sceneDepth, height, groundNormal.xy);
        return result;
    }

    // Surface normal from the neighbouring pixels on the same surface.
    float2 screenTexel = ScreenParams.xy;
    float3 surfaceDx = GetShorterDifference(surface, GetSurface(IN.uv + float2(screenTexel.x, 0.0f)), GetSurface(IN.uv - float2(screenTexel.x, 0.0f)));
    float3 surfaceDy = GetShorterDifference(surface, GetSurface(IN.uv + float2(0.0f, screenTexel.y)), GetSurface(IN.uv - float2(0.0f, screenTexel.y)));
    float3 surfaceNormal = normalize(cross(surfaceDx, surfaceDy));

    float alpha = IntersectionBlendFactor(height, sceneDepth, groundNormal, surfaceNormal, GroundParams.y, FieldParams.z);

    float3 groundPoint = foot;
    if (GroundParams.z > 0.5f)
        groundPoint += GetUnrollOffset(surface, surfaceNormal, groundNormal, height);

    FieldAttributes attributes = GetFieldAttributes(groundPoint);

    // Debug mode 7: vertex colour and blend weight 7 as plain floats, without packing.
    if (GroundParams.w > 0.5f) {
        result.target0 = float4(groundPoint, PackUnorm2x12(groundNormal.xy * 0.5f + 0.5f));
        result.target1 = float4(attributes.vertexColor, attributes.blend1.z);
        return result;
    }

    result.target0 = float4(groundPoint, PackUnorm2x12(groundNormal.xy * 0.5f + 0.5f));
    result.target1 = float4(
        PackUnorm3x8(attributes.blend0.xyz),
        PackUnorm3x8(float3(attributes.blend0.w, attributes.blend1.xy)),
        PackUnorm3x8(float3(attributes.blend1.z, attributes.vertexColor.rg)),
        PackUnorm3x8(float3(attributes.vertexColor.b, alpha, 0.0f)));
    return result;
}

#endif // PS
