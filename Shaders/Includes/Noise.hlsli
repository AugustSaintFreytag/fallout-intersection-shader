// World-space value noise, shared by the topper crop and the hard edge of the terrain overlay.

// Returns a hash in [0, 1) of a 2D lattice point.
// Reference at "Hash without Sine" by Dave Hoskins, "hash12".
float Hash(float2 lattice) {
    float3 p = frac(lattice.xyx * 0.1031f);
    p += dot(p, p.yzx + 33.33f);
    return frac((p.x + p.y) * p.z);
}

// Returns smooth value noise in [0, 1] with lattice spacing 1.
float ValueNoise(float2 position) {
    float2 lattice = floor(position);
    float2 blend = position - lattice;
    blend = blend * blend * (3.0f - 2.0f * blend);
    float a = Hash(lattice);
    float b = Hash(lattice + float2(1.0f, 0.0f));
    float c = Hash(lattice + float2(0.0f, 1.0f));
    float d = Hash(lattice + float2(1.0f, 1.0f));
    return lerp(lerp(a, b, blend.x), lerp(c, d, blend.x), blend.y);
}

// Returns two octaves of value noise in [0, 1] at a world position. The first octave has the given feature size.
// World coordinates are wrapped to keep the hash input small.
float WorldNoise(float2 world, float featureSize) {
    float2 position = frac(world / 65536.0f) * 65536.0f / featureSize;
    return ValueNoise(position) * 0.65f + ValueNoise(position * 2.03f + 17.0f) * 0.35f;
}
