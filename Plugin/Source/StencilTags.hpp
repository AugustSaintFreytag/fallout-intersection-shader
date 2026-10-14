#pragma once

class BSRenderedTexture;

// Tags the pixels of statics that may blend with terrain in the stencil buffer while the opaque world is drawn.
//
// The opaque stage (BSShaderAccumulator::FinishAccumulating_Standard_PreResolveDepth, GAME - 0xB65AE0) runs with
// stencil writes forced on. Every batch-rendered pass replaces our bit: eligible statics write it, everything else
// (terrain, actors, other statics) clears it again where it is drawn in front. The overlay pass then only draws
// where the bit is set, and clears the stencil buffer afterwards.
//
// Eligibility: the reference's base form type is in the INI type list and its form ID is not blacklisted.
namespace StencilTags {
	// The game only uses bits 0x01, 0x02 and 0x04 (water), so we take the top bit.
	static constexpr uint32_t TERRAIN_BLEND_BIT = 0x80;

	void InstallHooks();

	// Resolves the INI selection against the load order. Call once the game data is loaded (NVSE DeferredInit).
	void LoadSelection();

	// Enables or disables tagging, for example when the overlay shaders are missing.
	void SetEnabled(bool abEnabled);

	// Wrap every batch-rendered pass draw. Returns whether EndPass has to be called after the draw.
	bool BeginPass(const void* apRenderPass);
	void EndPass();

	// Number of opaque stages tagged since the last call. For diagnostics.
	uint32_t ConsumeTaggedStageCount();

	// Whether apTarget's stencil buffer holds tags from this frame's opaque stage.
	bool HasTags(const BSRenderedTexture* apTarget);

	// Clears the stencil buffer of the bound target and forgets the tags.
	void Clear(IDirect3DDevice9* apDevice);
}
