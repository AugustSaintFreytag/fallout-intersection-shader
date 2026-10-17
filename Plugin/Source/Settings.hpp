#pragma once

#include <string>
#include <vector>

namespace Settings {

	enum DebugMode : uint32_t {
		DEBUG_NONE = 0,
		DEBUG_DEPTH_BANDS = 1, // Tints the scene by linearized depth. Checks hook timing, depth access and linearization.
		DEBUG_OVERLAY_MASK = 2, // Draws the terrain overlay in a colour per landscape block at its blend opacity.
		DEBUG_STENCIL_TAGS = 3, // Tints every pixel tagged for terrain blending.
		DEBUG_GROUND_HEIGHT = 4, // Tints every surface by its height above the terrain under it. Reads the ground buffer.
		DEBUG_VERTEX_COLOR = 5, // Draws the vertex colour at the ground point over tagged statics.
		DEBUG_BLEND_WEIGHTS = 6, // Draws the strongest blend weight at the ground point over tagged statics.
		DEBUG_RAW_VERTEX_COLOR = 7, // Draws the vertex colour at the ground point as written by the ground pass, unpacked.
		DEBUG_TOPPER_TAGS = 8, // Tints every pixel tagged as a topper for static-to-static blending.
		DEBUG_TOPPER_BLEND = 9, // Paints the coverage of the topper blend red.
		DEBUG_TOPPER_BLEND_CAUSE = 10, // Paints every topper blend candidate by why it blends or not.
		DEBUG_EDGE_FEATURES = 11, // Draws the texture features of the terrain edge over the blend band.
	};

	inline bool bEnabled = true;
	inline uint32_t uiToggleKey = 121; // Windows virtual-key code, 121 = F10. 0 turns the key off.
	inline uint32_t uiReloadKey = 122; // Windows virtual-key code, 122 = F11. 0 turns the key off.
	inline bool bUnrolledMapping = true; // Off: the blend samples the terrain straight under the surface.
	inline float fMaxDistance = 4096.0f; // Game units, ~58 m
	inline float fBlendRange = 48.0f; // Game units along the surface, ~70 cm
	inline float fBlendClimbHeight = 12.0f; // Game units above the terrain, ~17 cm
	inline float fEdgeSoftness = 0.5f; // 0-1, width of the terrain fade in blend amount. 1 fades over the whole range.
	inline float fEdgeFeatureStrength = 0.75f; // 0-2, how far texture features shift the terrain edge
	inline float fEdgeFeatureSize = 12.0f; // Game units, ~17 cm. Blur of the height that the features stand out from.
	inline float fContactShadow = 0.4f; // 0-1, darkening of the static where the terrain blend meets it
	inline float fContactShadowWidth = 0.25f; // 0.05-0.5, share of the blend range the contact shadow fades over
	inline float fStaticBlendRange = 6.0f; // Game units above the floor, ~9 cm. 0 turns the topper blend off.
	inline float fStaticContactShadowFactor = 1.0f; // 0-1, factor on `fContactShadow` for toppers
	inline std::vector<uint8_t> kBlendTypesTerrainToStatic; // Base form type IDs
	inline std::vector<std::string> kTerrainToStaticBlacklist; // Editor IDs of base forms
	inline std::vector<std::string> kStaticToStaticWhitelist; // Editor IDs of base forms, the toppers
	inline DebugMode eDebugMode = DEBUG_NONE;
	inline bool bProfile = false; // Logs the GPU time of each stage every few seconds.
	inline bool bDeferToppers = true; // Off: toppers draw in the opaque stage.

	void Load();
	void LoadLive();

}
