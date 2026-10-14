#pragma once

class BSShaderAccumulator;
class BSRenderedTexture;

// The terrain overlay pass, run between Depth Resolve's depth copy and the post-depth effects (AO).
//
// The landscape passes recorded during the opaque pass are drawn again, without depth test and with alpha blending.
// Our terrain pixel shaders (shaders/terrain) switch into overlay mode for this replay: they compare their depth
// against the scene depth and output an opacity that falls off with the height of the scene surface above the terrain.
namespace OverlayPass {
	// Call once the D3D device exists (NVSE DeferredInit).
	void Initialize();

	// Call once per main loop iteration, before the frame is rendered. Logs changes in the render flow, such as
	// frames without a post-resolve callback while the game is paused.
	void OnMainLoop();

	void __cdecl OnPostResolveDepth(BSShaderAccumulator* apAccumulator, BSRenderedTexture* apTarget, IDirect3DBaseTexture9* apDepthTexture);
}
