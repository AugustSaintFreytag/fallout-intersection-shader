#pragma once

namespace Profiler {

	// Stages of the post-resolve callback, in the order they run.
	enum Stage : uint32_t {
		STAGE_FIELD		= 0,	// Height field and attribute field update
		STAGE_GROUND	= 1,	// Ground pass
		STAGE_OVERLAY	= 2,	// Overlay replay
		STAGE_COUNT
	};

	void Initialize();

	void BeginFrame(IDirect3DDevice9* apDevice);

	void EndStage(Stage aeStage);

	void EndFrame();

	void RecordReplay(uint32_t auiPasses, uint32_t auiRedirected, double adCpuMilliseconds);

}
