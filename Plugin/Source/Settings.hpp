#pragma once

#include <string>
#include <vector>

namespace Settings {

	enum DebugMode : uint32_t {
		DEBUG_NONE			= 0,
		DEBUG_DEPTH_BANDS	= 1, // Tints the scene by linearized depth. Checks hook timing, depth access and linearization.
		DEBUG_OVERLAY_MASK	= 2, // Draws the terrain overlay in a colour per landscape block at its blend opacity.
		DEBUG_STENCIL_TAGS	= 3, // Tints every pixel tagged for terrain blending.
		DEBUG_GROUND_HEIGHT	= 4, // Tints every surface by its height above the terrain under it. Reads the ground buffer.
		DEBUG_VERTEX_COLOR	= 5, // Draws the vertex colour at the ground point over tagged statics.
		DEBUG_BLEND_WEIGHTS	= 6, // Draws the strongest blend weight at the ground point over tagged statics.
		DEBUG_RAW_VERTEX_COLOR	= 7, // Draws the vertex colour at the ground point as written by the ground pass, unpacked.
		DEBUG_TOPPER_TAGS	= 8, // Tints every pixel tagged as a topper for static-to-static blending.
		DEBUG_TOPPER_CROP	= 9, // Paints the pixels the topper crop cuts away red.
		DEBUG_TOPPER_CROP_CAUSE	= 10, // Paints every crop candidate by why it is cropped or not.
	};

	inline bool						bEnabled				= true;
	inline bool						bUnrolledMapping		= true;		// Off: the blend samples the terrain straight under the surface.
	inline float					fMaxDistance			= 4096.0f;	// Game units, ~58 m
	inline float					fBlendRange				= 48.0f;	// Game units along the surface, ~70 cm
	inline float					fBlendRangeOrthoFactor	= 0.5f;		// 0-1, factor on `fBlendRange` for faces orthogonal to the terrain
	inline float					fCropHeight				= 6.0f;		// Game units above the floor, ~9 cm. 0 turns the crop off.
	inline float					fCropScale				= 8.0f;		// Game units, size of the crop noise features
	inline std::vector<uint8_t>		kTerrainBlendTypes;					// Base form type IDs
	inline std::vector<std::string>	kTerrainToStaticBlacklist;			// Editor IDs of base forms
	inline std::vector<std::string>	kStaticToStaticWhitelist;			// Editor IDs of base forms, the toppers
	inline DebugMode				eDebugMode				= DEBUG_NONE;
	inline bool						bProfile				= false;	// Logs the GPU time of each stage every few seconds.
	inline bool						bDeferToppers			= true;		// Off: toppers draw in the opaque stage.

	void Load();

}
