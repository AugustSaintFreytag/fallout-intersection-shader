#include "TerrainPasses.hpp"

#include "StencilTags.hpp"

#include "Bethesda/BSShaderManager.hpp"

#include <vector>

namespace TerrainPasses {
	// Landscape pass types: 8 per texture count (1-7), for no point lights, 6, 12 and 24 point lights, each with and without canopy shadows.
	// 560 (the LOD fade pass) is left out on purpose.
	static constexpr uint32_t FIRST_LANDSCAPE_PASS	= 503;
	static constexpr uint32_t LAST_LANDSCAPE_PASS	= 558;

	// BSBatchRenderer's call to the per-pass draw function, in its pass group loop (GAME - 0xB9A120).
	static constexpr uint32_t RENDER_PASS_CALL		= 0xB9A2A1;

	// The per-pass draw function (GAME - 0xB994F0): cdecl, (RenderPass*, pass type, 3 more values), no return value.
	typedef void(__cdecl* RenderPassFn)(void*, uint32_t, uint32_t, uint32_t, uint32_t);

	struct RecordedPass {
		void*						pRenderPass;
		uint32_t					uiPassType;
		uint32_t					uiArg2;
		uint32_t					uiArg3;
		uint32_t					uiArg4;
		const BSRenderedTexture*	pTarget;
	};

	static CallDetour					kRenderPassDetour;
	static std::vector<RecordedPass>	kRecordedPasses;

	static void __cdecl RenderPassHook(void* apRenderPass, uint32_t auiPassType, uint32_t auiArg2, uint32_t auiArg3, uint32_t auiArg4) {
		if (auiPassType >= FIRST_LANDSCAPE_PASS && auiPassType <= LAST_LANDSCAPE_PASS)
			kRecordedPasses.push_back({ apRenderPass, auiPassType, auiArg2, auiArg3, auiArg4, BSShaderManager::GetCurrentRenderTarget() });

		const bool bTagged = StencilTags::BeginPass(apRenderPass);
		reinterpret_cast<RenderPassFn>(kRenderPassDetour.GetOverwrittenAddr())(apRenderPass, auiPassType, auiArg2, auiArg3, auiArg4);
		if (bTagged)
			StencilTags::EndPass();
	}

	void InstallHooks() {
		kRecordedPasses.reserve(256);
		kRenderPassDetour.ReplaceCall(RENDER_PASS_CALL, RenderPassHook);
		_MESSAGE("Landscape pass recorder installed (call at %08X -> %08X)", RENDER_PASS_CALL, kRenderPassDetour.GetOverwrittenAddr());
	}

	void Clear() {
		kRecordedPasses.clear();
	}

	uint32_t Replay(const BSRenderedTexture* apTarget) {
		auto pRenderPass = reinterpret_cast<RenderPassFn>(kRenderPassDetour.GetOverwrittenAddr());

		uint32_t uiCount = 0;
		for (const RecordedPass& rPass : kRecordedPasses) {
			// Reflections draw the landscape into their own targets.
			if (rPass.pTarget != apTarget)
				continue;

			pRenderPass(rPass.pRenderPass, rPass.uiPassType, rPass.uiArg2, rPass.uiArg3, rPass.uiArg4);
			uiCount++;
		}

		return uiCount;
	}
}
