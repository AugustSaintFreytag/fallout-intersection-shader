// Packs small values into float32 render target channels for the ground buffers.
//
// A float32 holds every integer below 2^24 exactly. One channel carries three 8-bit or two 12-bit values.
// Textures holding packed values must be point sampled, without sRGB conversion.
//
// Unpacking rounds with + 0.25, not + 0.5. Above 2^23 a float32 has no fraction bits. There, + 0.5 rounds to the
// nearest even integer. The largest packed value 2^24 - 1 then becomes 2^24 and carries into the top byte.

float PackUnorm3x8(float3 value) {
    float3 quantized = floor(saturate(value) * 255.0f + 0.5f);
    return quantized.x * 65536.0f + quantized.y * 256.0f + quantized.z;
}

float3 UnpackUnorm3x8(float packed) {
    float rest = floor(packed + 0.25f);
    float x = floor(rest / 65536.0f);
    rest -= x * 65536.0f;
    float y = floor(rest / 256.0f);
    return float3(x, y, rest - y * 256.0f) / 255.0f;
}

float PackUnorm2x12(float2 value) {
    float2 quantized = floor(saturate(value) * 4095.0f + 0.5f);
    return quantized.x * 4096.0f + quantized.y;
}

float2 UnpackUnorm2x12(float packed) {
    float rest = floor(packed + 0.25f);
    float x = floor(rest / 4096.0f);
    return float2(x, rest - x * 4096.0f) / 4095.0f;
}

// Returns the grid index of a landscape block modulo 4 on both axes, as a unorm value.
// The attribute fields store it per texel. Lookups can then leave out texels of a neighbouring block.
// Their blend weights refer to another texture set.
float GetBlockCode(float2 block) {
    float2 wrapped = floor(frac(block / 4.0f) * 4.0f + 0.5f);
    return (wrapped.x + wrapped.y * 4.0f) / 15.0f;
}

bool IsSameBlockCode(float storedCode, float code) {
    return abs(storedCode - code) * 15.0f < 0.5f;
}
