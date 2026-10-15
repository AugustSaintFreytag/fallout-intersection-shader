#pragma once

namespace DrawRedirect {

	void Initialize();

	bool IsReady();

	// Draw calls seen while armed.
	struct Counts {
		uint32_t	uiDrawn;	// Full-screen triangles, one per pass
		uint32_t	uiSkipped;	// Further draws of the same passes
		uint32_t	uiForeign;	// Skipped draws with another vertex buffer than their pass's first draw
	};

	void SetArmed(bool abArmed);

	void BeginPass();

	Counts ConsumeCounts();

}
