// World-space noise for the edge shared by the topper crop and the terrain overlay (`GetEdgeReach`).

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

// Returns the distance to the nearest feature point and a hash of its cell, for cells of size 1.
// Each cell holds one feature point at a hashed position.
float2 CellularNoise(float2 position) {
    float2 cell = floor(position);
    float2 local = position - cell;
    float nearest = 8.0f;
    float id = 0.0f;
    [unroll] for (int y = -1; y <= 1; y++) {
        [unroll] for (int x = -1; x <= 1; x++) {
            float2 neighbour = cell + float2(x, y);
            float2 delta = float2(x, y) + float2(Hash(neighbour), Hash(neighbour + 57.0f)) - local;
            float distanceSquared = dot(delta, delta);
            if (distanceSquared < nearest) {
                nearest = distanceSquared;
                id = Hash(neighbour + 113.0f);
            }
        }
    }
    return float2(sqrt(nearest), id);
}

// Weight of the large value noise layer under the grains. Keeps the contour from running at one height.
static const float ClumpWeight = 0.35f;

// Size of the clumps in the value noise layer, in grain sizes.
static const float ClumpScale = 4.0f;

// Returns grain noise in [0, 1] at a world position, with grains of the given size.
// Each grain is a bump of its own hashed height, highest at its centre. Grains sit on a weaker layer of larger clumps.
// World coordinates are wrapped to keep the hash input small.
float GrainNoise(float2 world, float grainSize) {
    float2 position = frac(world / 65536.0f) * 65536.0f / grainSize;
    float2 grain = CellularNoise(position);
    float bump = saturate(1.0f - grain.x) * (0.5f + 0.5f * grain.y);
    return lerp(bump, ValueNoise(position / ClumpScale), ClumpWeight);
}
