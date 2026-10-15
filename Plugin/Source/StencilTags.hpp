#pragma once

class BSRenderedTexture;

namespace StencilTags {

	// The game only uses bits 0x01, 0x02 and 0x04, for water. The tags take the top two bits.
	static constexpr uint32_t TERRAIN_BLEND_BIT = 0x80;
	static constexpr uint32_t TOPPER_BIT		= 0x40;
	static constexpr uint32_t TAG_BITS			= TERRAIN_BLEND_BIT | TOPPER_BIT;

	void InstallHooks();

	void LoadSelection();

	void SetEnabled(bool abEnabled);

	bool BeginPass(const void* apRenderPass);
	void EndPass();

	uint32_t ConsumeTaggedStageCount();

	// Eligible geometry seen during tagging.
	struct GeometryCounts {
		uint32_t	uiTagged;
		uint32_t	uiToppers;	// Tagged as toppers, a part of the tagged geometry
		uint32_t	uiDistant;	// Beyond `fMaxDistance`, left untagged
	};

	GeometryCounts ConsumeGeometryCounts();

	bool HasTags(const BSRenderedTexture* apTarget);

	void Clear(IDirect3DDevice9* apDevice);

}
