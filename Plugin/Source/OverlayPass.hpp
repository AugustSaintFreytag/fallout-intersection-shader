#pragma once

class BSShaderAccumulator;
class BSRenderedTexture;

namespace OverlayPass {

	void Initialize();

	void OnMainLoop();

	void __cdecl OnPostResolveDepth(BSShaderAccumulator* apAccumulator, BSRenderedTexture* apTarget, IDirect3DBaseTexture9* apDepthTexture);

}
