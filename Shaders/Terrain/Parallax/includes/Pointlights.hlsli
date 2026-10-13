// Pointlight helper functions.

#if defined(__INTELLISENSE__)
    #include "Helpers.hlsli"
#endif

// Vanilla attenuation, lightVector not normalized.
float vanillaAtt(float3 lightVector, float radius) {
    const float3 att = lightVector / radius;
    return 1 - shades(att, att);
}
