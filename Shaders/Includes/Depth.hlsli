// Depth helpers for the reversed depth buffer set up by LOD Flicker Fix.
//
// LOD Flicker Fix projects with _33 = -n / (f - n) and _43 = n * f / (f - n). Depth is then
//   d = n * (f - z) / (z * (f - n))      (d = 1 at the near plane, 0 at the far plane)
// and inverted
//   z = n * f / (d * (f - n) + n).

// Returns the linear view depth of a reversed depth value.
// Parameters: x = near, y = far - near, z = near * far
float LinearizeDepth(float depth, float4 params) {
    return params.z / (depth * params.y + params.x);
}
