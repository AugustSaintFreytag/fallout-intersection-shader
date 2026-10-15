#pragma once

#include <string>
#include <vector>

namespace Settings {

	enum DebugMode : uint32_t {
		DEBUG_NONE			= 0,
		DEBUG_DEPTH_BANDS	= 1, // Tints the scene by linearized depth. Checks hook timing, depth access and linearization.
		DEBUG_OVERLAY_MASK	= 2, // Draws the terrain overlay in a solid color at its blend opacity.
		DEBUG_STENCIL_TAGS	= 3, // Tints every pixel tagged for terrain blending.
		DEBUG_GROUND_HEIGHT	= 4, // Tints every surface by its height above the terrain under it. Reads the ground buffer.
		DEBUG_VERTEX_COLOR	= 5, // Ground mode: draws the vertex colour at the ground point over tagged statics.
		DEBUG_BLEND_WEIGHTS	= 6, // Ground mode: draws the strongest blend weight at the ground point over tagged statics.
		DEBUG_RAW_VERTEX_COLOR	= 7, // Ground mode: draws the vertex colour at the ground point as written by the ground pass, unpacked.
	};

	// A base form by plugin name and form ID without the load order byte.
	// Resolved once the game data is loaded.
	struct FormEntry {
		std::string	sPlugin;
		uint32_t	uiFormID;
	};

	inline bool						bEnabled				= true;
	inline bool						bStencilTags			= true;		// Off: the overlay covers everything on the ground.
	inline bool						bLipping				= true;		// Off: the overlay shades the terrain behind the surface.
	inline bool						bUnrolledMapping		= true;		// Off: lipping samples the terrain straight under the surface.
	inline bool						bGroundShading			= false;	// Ground mode: shades from the landscape block under each static. Experimental.
	inline float					fMaxDistance			= 4096.0f;	// Game units, ~58 m
	inline float					fBlendRange				= 48.0f;	// Game units along the surface, ~70 cm
	inline float					fMaxRayGap				= 128.0f;	// Game units, ~1.8 m
	inline std::vector<uint8_t>		kTerrainBlendTypes;					// Base form type IDs
	inline std::vector<FormEntry>	kTerrainBlendBlacklist;
	inline DebugMode				eDebugMode				= DEBUG_NONE;
	inline bool						bProfile				= false;	// Logs the GPU time of each stage every few seconds.

	void Load();

}
