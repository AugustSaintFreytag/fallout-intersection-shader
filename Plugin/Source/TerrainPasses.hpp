#pragma once

#include <functional>

class BSRenderedTexture;
class NiGeometry;

namespace TerrainPasses {

	// Landscape pass types: 8 per texture count (1-7). Variants for no point lights and 6, 12 and 24 point lights,
	// each with and without canopy shadows. Pass 560, the LOD fade pass, is left out on purpose.
	//
	// Reference at "VanillaPlusTerrain/main.cpp", "AddPass_Landscape".
	inline constexpr uint32_t FIRST_LANDSCAPE_PASS = 503;
	inline constexpr uint32_t LAST_LANDSCAPE_PASS = 558;

	void InstallHooks();

	void DrawPass(void* apRenderPass, uint32_t auiPassType, uint32_t auiArg2, uint32_t auiArg3, uint32_t auiArg4);

	void Clear();

	// A filter that decides per recorded pass whether to draw it, by its landscape geometry.
	typedef std::function<bool(const NiGeometry* apGeometry)> PassFilter;

	// A callback run right before a replayed pass is drawn, with its landscape geometry and pass type.
	typedef std::function<void(const NiGeometry* apGeometry, uint32_t auiPassType)> PassCallback;

	uint32_t Replay(const BSRenderedTexture* apTarget, const PassFilter& arFilter = nullptr, const PassCallback& arBeforeDraw = nullptr);

	void LogRecorded(const BSRenderedTexture* apTarget);

	bool Any(const BSRenderedTexture* apTarget, const PassFilter& arFilter);

}
