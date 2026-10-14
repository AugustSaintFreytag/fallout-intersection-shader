#pragma once

#include <string>
#include <vector>

namespace Settings {
	enum DebugMode : uint32_t {
		DEBUG_NONE			= 0,
		DEBUG_DEPTH_BANDS	= 1, // Tints the scene by linearized depth: hook timing, depth access and linearization.
		DEBUG_OVERLAY_MASK	= 2, // Draws the terrain overlay in a solid color, at its blend opacity.
		DEBUG_STENCIL_TAGS	= 3, // Tints every pixel tagged for terrain blending.
	};

	// A base form by plugin name and form ID without the load order byte, resolved once the game data is loaded.
	struct FormEntry {
		std::string	sPlugin;
		uint32_t	uiFormID;
	};

	inline bool						bEnabled				= true;
	inline bool						bStencilTags			= true;		// Off: the overlay covers everything on the ground, as before milestone 3.
	inline bool						bLipping				= true;		// Off: the overlay shades the terrain behind the surface, as before.
	inline float					fMaxDistance			= 4096.0f;	// Game units (~58 m).
	inline float					fBlendRange				= 24.0f;	// Game units (~35 cm).
	inline float					fMaxRayGap				= 128.0f;	// Game units (~1.8 m).
	inline std::vector<uint8_t>		kTerrainBlendTypes;					// Base form type IDs.
	inline std::vector<FormEntry>	kTerrainBlendBlacklist;
	inline DebugMode				eDebugMode				= DEBUG_NONE;

	void Load();
}
