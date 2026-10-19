#pragma once

namespace PassLog {

	// Where a topper pass is drawn.
	enum Stage : uint32_t {
		STAGE_OPAQUE = 0, // Opaque stage, with `bDeferToppers=0`
		STAGE_DEPTH = 1, // Depth replay
		STAGE_COLOUR = 2, // Colour replay
		STAGE_FEATURES = 3, // Feature replay
	};

	void Arm();

	void OnMainLoop();

	bool IsCapturing();

	void BeginPass(const void* apRenderPass, uint32_t auiPassType, Stage aeStage);
	void EndPass();

	void OnDraw(IDirect3DDevice9* apDevice);

}
