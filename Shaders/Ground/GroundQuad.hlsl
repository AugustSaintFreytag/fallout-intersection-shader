// Vertex shader for ground mode (lipping mode 3) of the terrain overlay.
//
// During the overlay replay, the plugin draws one triangle over the whole screen with this shader in place of each
// landscape block's mesh (see Plugin/Source/DrawRedirect.hpp). The block's own pass has bound its pixel shader, textures
// and constants, so that shader runs on every pixel of the screen. In ground mode it takes everything from the ground
// buffers and keeps only the pixels whose ground point lies in this block. The interpolants it would read from the mesh
// are zero here.

struct VS_INPUT {
    float3 position : POSITION;
};

struct VS_OUTPUT {
    float4 position : POSITION;
    float4 texcoord0 : TEXCOORD0;
    float4 texcoord1 : TEXCOORD1;
    float4 texcoord2 : TEXCOORD2;
    float4 texcoord3 : TEXCOORD3;
    float4 texcoord4 : TEXCOORD4;
    float4 texcoord5 : TEXCOORD5;
    float4 texcoord6 : TEXCOORD6;
    float4 texcoord7 : TEXCOORD7;
    float4 color0 : COLOR0;
    float4 color1 : COLOR1;
};

VS_OUTPUT main(VS_INPUT IN) {
    VS_OUTPUT OUT;
    OUT.position = float4(IN.position.xy, 0.0f, 1.0f);
    OUT.texcoord0 = 0.0f;
    OUT.texcoord1 = 0.0f;
    OUT.texcoord2 = 0.0f;
    OUT.texcoord3 = 0.0f;
    OUT.texcoord4 = 0.0f;
    OUT.texcoord5 = 0.0f;
    OUT.texcoord6 = 0.0f;
    OUT.texcoord7 = 0.0f;
    OUT.color0 = 0.0f;
    OUT.color1 = 0.0f;
    return OUT;
}
