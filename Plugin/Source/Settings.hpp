#pragma once

namespace Settings {
	enum DebugMode : uint32_t {
		DEBUG_NONE			= 0,
		DEBUG_DEPTH_BANDS	= 1, // Tints the scene by linearized depth: hook timing, depth access and linearization.
		DEBUG_OVERLAY_MASK	= 2, // Draws the terrain overlay in a solid color, at its blend opacity.
	};

	inline bool			bEnabled		= true;
	inline float		fMaxDistance	= 4096.0f;	// Game units (~58 m).
	inline float		fBlendRange		= 24.0f;	// Game units (~35 cm).
	inline float		fMaxRayGap		= 128.0f;	// Game units (~1.8 m).
	inline DebugMode	eDebugMode		= DEBUG_NONE;

	void Load();
}
