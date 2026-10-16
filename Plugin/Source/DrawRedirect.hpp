#pragma once

namespace DrawRedirect {

	void Initialize();

	bool IsReady();

	// Draw calls seen in full-screen mode.
	struct Counts {
		uint32_t uiDrawn; // Full-screen triangles, one per pass
		uint32_t uiSkipped; // Further draws of the same passes
		uint32_t uiForeign; // Skipped draws with another vertex buffer than their pass's first draw
	};

	enum Mode : uint32_t {
		MODE_OFF = 0, // Draws pass through.
		MODE_FULL_SCREEN = 1, // One full-screen triangle per pass with the pass's pixel shader, for the overlay
		MODE_SHADER = 2, // Every draw keeps its mesh and uses the pass's pixel shader, for the height field
	};

	void SetMode(Mode aeMode);

	void BeginPass(IDirect3DPixelShader9* apPixelShader);

	Counts ConsumeCounts();

}
