#pragma once

class BSRenderedTexture;

namespace StencilTags {

	// The game only uses bits 0x01, 0x02 and 0x04, for water. The tags take the top two bits.
	static constexpr uint32_t TERRAIN_BLEND_BIT = 0x80;
	static constexpr uint32_t TOPPER_BIT = 0x40;
	static constexpr uint32_t TAG_BITS = TERRAIN_BLEND_BIT | TOPPER_BIT;

	// Marks a topper pixel in front of a base with `TERRAIN_BLEND_BIT`, for the topper blend. Set between the topper
	// replays, because the colour replay replaces the base's `TERRAIN_BLEND_BIT` with the topper's own.
	static constexpr uint32_t TOPPER_BASE_BIT = 0x20;

	void InstallHooks();

	void LoadSelection();

	void SetEnabled(bool abEnabled);

	uint32_t BeginPass(const void* apRenderPass);
	void EndPass();

	bool KeepsTags();

	uint32_t ConsumeTaggedStageCount();

	// Eligible geometry seen during tagging.
	struct GeometryCounts {
		uint32_t uiTagged;
		uint32_t uiToppers; // Tagged as toppers, a part of the tagged geometry
		uint32_t uiDistant; // Beyond `fMaxDistance`, left untagged
	};

	GeometryCounts ConsumeGeometryCounts();

	bool HasTags(const BSRenderedTexture* apTarget);

	void Clear(IDirect3DDevice9* apDevice);

}
