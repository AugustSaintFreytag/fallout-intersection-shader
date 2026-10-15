#pragma once

class BSRenderedTexture;

namespace StencilTags {

	// The game only uses bits 0x01, 0x02 and 0x04, for water. The tag takes the top bit.
	static constexpr uint32_t TERRAIN_BLEND_BIT = 0x80;

	void InstallHooks();

	void LoadSelection();

	void SetEnabled(bool abEnabled);

	bool BeginPass(const void* apRenderPass);
	void EndPass();

	uint32_t ConsumeTaggedStageCount();

	bool HasTags(const BSRenderedTexture* apTarget);

	void Clear(IDirect3DDevice9* apDevice);

}
