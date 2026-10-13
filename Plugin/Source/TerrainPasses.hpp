#pragma once

class BSRenderedTexture;

// Records the landscape render passes as the batch renderer draws them, so they can be drawn again later in the frame.
//
// BSBatchRenderer unlinks every pass from its pass group once it is drawn (0xB9A2B4-0xB9A309), so calling
// RenderBatches again finds nothing. The RenderPass objects themselves belong to the geometry's shader property
// and stay valid until the next accumulation, so we keep the arguments of each landscape pass draw instead.
namespace TerrainPasses {
	void InstallHooks();

	// Drops all recorded passes. Called once per frame before rendering.
	void Clear();

	// Draws the landscape passes recorded into apTarget again, with whatever state the caller has forced.
	// Returns the number of passes drawn.
	uint32_t Replay(const BSRenderedTexture* apTarget);
}
