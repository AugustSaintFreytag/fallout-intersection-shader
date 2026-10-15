#pragma once

namespace Profiler {

	// Stages of the post-resolve callback, in the order they run.
	enum Stage : uint32_t {
		STAGE_TOPPERS	= 0,	// Topper replay and depth resolve
		STAGE_FIELD		= 1,	// Height field and attribute field update
		STAGE_GROUND	= 2,	// Ground pass
		STAGE_OVERLAY	= 3,	// Overlay replay
		STAGE_COUNT
	};

	void Initialize();

	void BeginFrame(IDirect3DDevice9* apDevice);

	void EndStage(Stage aeStage);

	void EndFrame();

	void RecordReplay(uint32_t auiPasses, uint32_t auiDrawn, uint32_t auiSkipped, uint32_t auiForeign, double adScreens, double adCpuMilliseconds);

	void RecordTags(uint32_t auiTagged, uint32_t auiToppers, uint32_t auiDistant);

}
