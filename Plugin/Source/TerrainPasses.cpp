#include "TerrainPasses.hpp"

#include "StencilTags.hpp"
#include "TopperPasses.hpp"

#include "Bethesda/BSShaderManager.hpp"
#include "Bethesda/BSShaderProperty.hpp"
#include "Gamebryo/NiGeometry.hpp"
#include "Gamebryo/NiNode.hpp"

#include <vector>

// Records the landscape render passes as the batch renderer draws them, for drawing again later in the frame.
//
// `BSBatchRenderer` unlinks every pass from its pass group once it is drawn (GAME - 0xB9A2B4 to 0xB9A309).
// Calling `RenderBatches` again finds nothing. The `RenderPass` objects belong to the geometry's shader property and
// stay valid until the next accumulation. The recorder keeps the arguments of each landscape pass draw instead.
namespace TerrainPasses {

	// Call of the per-pass draw function in the pass group loop of `BSBatchRenderer` (GAME - 0xB9A120).
	static constexpr uint32_t RENDER_PASS_CALL = 0xB9A2A1;

	// Per-pass draw function (GAME - 0xB994F0), cdecl `void (RenderPass*, pass type, 3 more values)`.
	typedef void(__cdecl* RenderPassFn)(void*, uint32_t, uint32_t, uint32_t, uint32_t);

	// A landscape pass draw recorded for the replay.
	struct RecordedPass {
		void* pRenderPass;
		uint32_t uiPassType;
		uint32_t uiArg2;
		uint32_t uiArg3;
		uint32_t uiArg4;
		const BSRenderedTexture* pTarget;
	};

	static CallDetour kRenderPassDetour;
	static std::vector<RecordedPass> kRecordedPasses;

	// Records landscape passes, defers topper passes and wraps every other pass draw with stencil tagging.
	static void __cdecl RenderPassHook(void* apRenderPass, uint32_t auiPassType, uint32_t auiArg2, uint32_t auiArg3, uint32_t auiArg4) {
		if (auiPassType >= FIRST_LANDSCAPE_PASS && auiPassType <= LAST_LANDSCAPE_PASS)
			kRecordedPasses.push_back({ apRenderPass, auiPassType, auiArg2, auiArg3, auiArg4, BSShaderManager::GetCurrentRenderTarget() });

		const uint32_t uiTags = StencilTags::BeginPass(apRenderPass);
		if ((uiTags & StencilTags::TOPPER_BIT) && TopperPasses::Defer(apRenderPass, auiPassType, auiArg2, auiArg3, auiArg4, uiTags)) {
			StencilTags::EndPass();
			return;
		}

		DrawPass(apRenderPass, auiPassType, auiArg2, auiArg3, auiArg4);
		if (uiTags)
			StencilTags::EndPass();
	}

	// Draws a render pass with the per-pass draw function of the batch renderer, past the hook.
	void DrawPass(void* apRenderPass, uint32_t auiPassType, uint32_t auiArg2, uint32_t auiArg3, uint32_t auiArg4) {
		reinterpret_cast<RenderPassFn>(kRenderPassDetour.GetOverwrittenAddr())(apRenderPass, auiPassType, auiArg2, auiArg3, auiArg4);
	}

	// Hooks the per-pass draw call of the batch renderer.
	void InstallHooks() {
		kRecordedPasses.reserve(256);
		kRenderPassDetour.ReplaceCall(RENDER_PASS_CALL, RenderPassHook);
		_MESSAGE("Landscape pass recorder installed (call at %08X -> %08X)", RENDER_PASS_CALL, kRenderPassDetour.GetOverwrittenAddr());
	}

	// Drops all recorded passes. Called once per frame before rendering.
	void Clear() {
		kRecordedPasses.clear();
	}

	static const NiGeometry* GetGeometry(const RecordedPass& arPass) {
		return static_cast<const BSShaderProperty::RenderPass*>(arPass.pRenderPass)->pGeometry;
	}

	// Draws the landscape passes recorded into the given target again, with the state the caller has forced.
	// Only passes accepted by the filter are drawn, if one is given. Calls the callback before each draw.
	// Returns the number of passes drawn.
	uint32_t Replay(const BSRenderedTexture* apTarget, const PassFilter& arFilter, const PassCallback& arBeforeDraw) {
		uint32_t uiCount = 0;
		for (const RecordedPass& rPass : kRecordedPasses) {
			// Reflections draw the landscape into their own targets.
			if (rPass.pTarget != apTarget)
				continue;
			if (arFilter && !arFilter(GetGeometry(rPass)))
				continue;
			if (arBeforeDraw)
				arBeforeDraw(GetGeometry(rPass), rPass.uiPassType);

			DrawPass(rPass.pRenderPass, rPass.uiPassType, rPass.uiArg2, rPass.uiArg3, rPass.uiArg4);
			uiCount++;
		}

		return uiCount;
	}

	// Logs every landscape pass recorded into the given target with pass type, geometry, parent and world bound.
	void LogRecorded(const BSRenderedTexture* apTarget) {
		for (const RecordedPass& rPass : kRecordedPasses) {
			if (rPass.pTarget != apTarget)
				continue;

			const NiGeometry* pGeometry = GetGeometry(rPass);
			const NiNode* pParent = pGeometry ? pGeometry->m_pkParent : nullptr;
			const NiBound* pBound = pGeometry ? pGeometry->m_pWorldBound : nullptr;
			_MESSAGE("  pass %u, geometry %p \"%s\", parent \"%s\", bound centre %.0f, %.0f, %.0f, radius %.0f", rPass.uiPassType, pGeometry,
				pGeometry && pGeometry->GetName() ? pGeometry->GetName() : "", pParent && pParent->GetName() ? pParent->GetName() : "",
				pBound ? pBound->m_kCenter.x : 0.0f, pBound ? pBound->m_kCenter.y : 0.0f, pBound ? pBound->m_kCenter.z : 0.0f, pBound ? pBound->m_fRadius : 0.0f);
		}
	}

	// Returns whether the filter accepts any landscape pass recorded into the given target.
	bool Any(const BSRenderedTexture* apTarget, const PassFilter& arFilter) {
		for (const RecordedPass& rPass : kRecordedPasses) {
			if (rPass.pTarget == apTarget && arFilter(GetGeometry(rPass)))
				return true;
		}
		return false;
	}

}
