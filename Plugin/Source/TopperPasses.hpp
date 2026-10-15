#pragma once

class BSRenderedTexture;
class BSShaderAccumulator;

namespace TopperPasses {

	void Initialize();

	bool Defer(void* apRenderPass, uint32_t auiPassType, uint32_t auiArg2, uint32_t auiArg3, uint32_t auiArg4, uint32_t auiTags);

	uint32_t Replay(BSShaderAccumulator* apAccumulator, BSRenderedTexture* apTarget, IDirect3DBaseTexture9* apDepthTexture);

	void Clear();

}
