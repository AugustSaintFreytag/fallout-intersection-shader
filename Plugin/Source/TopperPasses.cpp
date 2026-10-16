#include "TopperPasses.hpp"

#include "Dependencies.hpp"
#include "RenderStates.hpp"
#include "StencilTags.hpp"
#include "Settings.hpp"
#include "TerrainPasses.hpp"
#include "TopperCrop.hpp"

#include "Bethesda/BSShaderAccumulator.hpp"
#include "Bethesda/BSShaderManager.hpp"
#include "Gamebryo/NiDX9Renderer.hpp"

#include <algorithm>
#include <utility>
#include <vector>

// Draws the toppers after the opaque stage instead of during it.
//
// The per-pass hook records topper passes in the opaque stage and skips their draws. The bases behind the toppers are
// then fully drawn, and Depth Resolve's depth copy holds them without the toppers. The post-resolve callback draws the
// toppers first and has Depth Resolve copy the depth again. The ground pass, AO and everything after them see the toppers.
//
// The passes are drawn twice. The first replay writes depth and `TOPPER_BIT` only, with colour writes off.
// `TERRAIN_BLEND_BIT` of the base under each topper pixel stays. `TopperCrop` then crops the toppers where they rest
// on a floor. The second replay draws colour where `TOPPER_BIT` is still set and writes the topper's own
// `TERRAIN_BLEND_BIT`. It draws the same geometry with the same shaders, so its depth equals the depth of the first
// replay and passes the depth test of each pass.
//
// A pass inherits render states from the passes before it and from its pass group. `BSBatchRenderer` locks alpha test
// and alpha to coverage for each pass group (GAME - 0xB9A120). The recorder keeps the states a pass starts with.
// The replay sets them again before each pass. It forces the locked ones, so the pass cannot change them.
// Afterwards the replay ends the shader technique of the last pass like `RenderBatches` does, and restores the states
// it set to their values from before the replay.
//
// Reference at the pass group loop of `BSBatchRenderer` (GAME - 0xB9A120).
// Reference at `BSShaderAccumulator::RenderBatches` (GAME - 0xB63770), call at 0xB639C6.
namespace TopperPasses {

	// States a pass inherits and may change.
	static constexpr D3DRENDERSTATETYPE kInheritedStates[] = {
		D3DRS_ZENABLE, D3DRS_ZWRITEENABLE, D3DRS_ALPHAREF, D3DRS_ALPHAFUNC, D3DRS_ALPHABLENDENABLE,
		D3DRS_SRCBLEND, D3DRS_DESTBLEND, D3DRS_BLENDOP, D3DRS_CULLMODE, D3DRS_COLORWRITEENABLE,
	};

	// States the pass group locks. Alpha to coverage is `D3DRS_ADAPTIVETESS_Y` on NVIDIA and `D3DRS_POINTSIZE` on AMD
	// (GAME - 0xB98540).
	static constexpr D3DRENDERSTATETYPE kLockedStates[] = { D3DRS_ALPHATESTENABLE, D3DRS_ADAPTIVETESS_Y, D3DRS_POINTSIZE };

	static constexpr uint32_t TEXTURE_STAGE_COUNT = 16;

	// Ends the technique of the current shader and pass type, and clears both (GAME - 0xB988E0). cdecl, no arguments.
	// The per-pass draw function starts a technique when the shader or pass type changes (GAME - 0xB99390).
	static constexpr uint32_t END_TECHNIQUE = 0xB988E0;

	// A topper pass draw recorded for the replay, with the render states it started with.
	struct RecordedPass {
		void*						pRenderPass;
		uint32_t					uiPassType;
		uint32_t					uiArg2;
		uint32_t					uiArg3;
		uint32_t					uiArg4;
		const BSRenderedTexture*	pTarget;
		uint32_t					uiTags;
		uint32_t					uiZFunc;
		uint32_t					uiInherited[ARRAYSIZE(kInheritedStates)];
		uint32_t					uiLocked[ARRAYSIZE(kLockedStates)];
	};

	static bool							bEnabled	= false;
	static std::vector<RecordedPass>	kRecordedPasses;
	static double						dResolveMilliseconds	= 0.0;	// CPU time in `ResolveSceneDepth` since the last `ConsumeResolveMilliseconds`

	// Resolves the scene depth through Depth Resolve and adds the CPU time of the call to the profile.
	static void ResolveSceneDepth(BSShaderAccumulator* apAccumulator, BSRenderedTexture* apTarget) {
		LARGE_INTEGER kStart, kEnd, kFrequency;
		QueryPerformanceCounter(&kStart);
		Dependencies::ResolveSceneDepth(apAccumulator, apTarget);
		QueryPerformanceCounter(&kEnd);
		QueryPerformanceFrequency(&kFrequency);
		dResolveMilliseconds += static_cast<double>(kEnd.QuadPart - kStart.QuadPart) * 1000.0 / static_cast<double>(kFrequency.QuadPart);
	}

	// Enables deferral if Depth Resolve can resolve the depth again. Toppers are drawn in the opaque stage otherwise.
	// Call at NVSE DeferredInit.
	void Initialize() {
		bEnabled = Settings::bDeferToppers && Dependencies::CanResolveSceneDepth();
		kRecordedPasses.reserve(256);
		if (!Settings::bDeferToppers)
			_MESSAGE("Toppers are drawn in the opaque stage: bDeferToppers is off");
		else if (!bEnabled)
			_MESSAGE("Toppers are drawn in the opaque stage: Depth Resolve does not provide ResolveSceneDepth");
	}

	// Sets `D3DRS_ZFUNC` on the device and in the render state cache.
	// LOD Flicker Fix remaps every value set through `NiDX9RenderState`. The recorded values are remapped already.
	static void SetZFunc(NiDX9RenderState* apRenderState, IDirect3DDevice9* apDevice, uint32_t auiValue) {
		apDevice->SetRenderState(D3DRS_ZFUNC, auiValue);
		apRenderState->m_akRenderStateSettings[D3DRS_ZFUNC].m_uiCurrValue = auiValue;
	}

	// Records a topper pass for the replay instead of drawing it. Returns false if it has to be drawn now.
	bool Defer(void* apRenderPass, uint32_t auiPassType, uint32_t auiArg2, uint32_t auiArg3, uint32_t auiArg4, uint32_t auiTags) {
		if (!bEnabled)
			return false;

		const NiDX9RenderState* pRenderState = NiDX9Renderer::GetSingleton()->m_pkRenderState;
		RecordedPass kPass = { apRenderPass, auiPassType, auiArg2, auiArg3, auiArg4, BSShaderManager::GetCurrentRenderTarget(), auiTags,
			pRenderState->m_akRenderStateSettings[D3DRS_ZFUNC].m_uiCurrValue };
		for (uint32_t i = 0; i < ARRAYSIZE(kInheritedStates); i++)
			kPass.uiInherited[i] = pRenderState->m_akRenderStateSettings[kInheritedStates[i]].m_uiCurrValue;
		for (uint32_t i = 0; i < ARRAYSIZE(kLockedStates); i++)
			kPass.uiLocked[i] = pRenderState->m_akRenderStateSettings[kLockedStates[i]].m_uiCurrValue;

		kRecordedPasses.push_back(kPass);
		return true;
	}

	// Draws the topper passes deferred from the opaque stage of the given target, cropped on floors, then resolves the
	// scene depth again. The given texture holds the scene depth from Depth Resolve's first resolve.
	// Returns the number of passes per replay.
	uint32_t Replay(BSShaderAccumulator* apAccumulator, BSRenderedTexture* apTarget, IDirect3DBaseTexture9* apDepthTexture) {
		const auto itFirst = std::find_if(kRecordedPasses.begin(), kRecordedPasses.end(), [apTarget](const RecordedPass& arPass) { return arPass.pTarget == apTarget; });
		if (itFirst == kRecordedPasses.end())
			return 0;

		NiDX9RenderState* pRenderState = NiDX9Renderer::GetSingleton()->m_pkRenderState;
		IDirect3DDevice9* pDevice = NiDX9Renderer::GetSingleton()->GetD3DDevice();
		const bool bCrop = TopperCrop::CopyBaseDepth(pDevice, apDepthTexture);
		RenderStates::SyncTextureStages(pRenderState, pDevice, TEXTURE_STAGE_COUNT);

		// The state the opaque stage left, restored after the replay.
		const uint32_t uiSavedZFunc = pRenderState->m_akRenderStateSettings[D3DRS_ZFUNC].m_uiCurrValue;
		uint32_t uiSavedInherited[ARRAYSIZE(kInheritedStates)];
		for (uint32_t i = 0; i < ARRAYSIZE(kInheritedStates); i++)
			uiSavedInherited[i] = pRenderState->m_akRenderStateSettings[kInheritedStates[i]].m_uiCurrValue;

		uint32_t uiCount = 0;
		{
			// The stencil reference is the tag bits of each pass. The masks pick the bits each replay tests and writes.
			ForcedRenderStates kStates(pRenderState);
			kStates.Force(D3DRS_STENCILENABLE, TRUE);
			kStates.Force(D3DRS_TWOSIDEDSTENCILMODE, FALSE);
			kStates.Force(D3DRS_STENCILFUNC, D3DCMP_ALWAYS);
			kStates.Force(D3DRS_STENCILMASK, StencilTags::TOPPER_BIT);
			kStates.Force(D3DRS_STENCILPASS, D3DSTENCILOP_REPLACE);
			kStates.Force(D3DRS_STENCILFAIL, D3DSTENCILOP_KEEP);
			kStates.Force(D3DRS_STENCILZFAIL, D3DSTENCILOP_KEEP);
			kStates.Force(D3DRS_STENCILWRITEMASK, StencilTags::TOPPER_BIT);
			kStates.Force(D3DRS_STENCILREF, 0);
			for (uint32_t i = 0; i < ARRAYSIZE(kLockedStates); i++)
				kStates.Force(kLockedStates[i], itFirst->uiLocked[i]);

			const auto DrawPasses = [&]() {
				uint32_t uiDrawn = 0;
				for (auto it = itFirst; it != kRecordedPasses.end(); ++it) {
					if (it->pTarget != apTarget)
						continue;

					for (uint32_t i = 0; i < ARRAYSIZE(kInheritedStates); i++)
						pRenderState->SetRenderState(kInheritedStates[i], it->uiInherited[i], 0, false);
					for (uint32_t i = 0; i < ARRAYSIZE(kLockedStates); i++)
						kStates.Update(kLockedStates[i], it->uiLocked[i]);
					kStates.Update(D3DRS_STENCILREF, it->uiTags);
					SetZFunc(pRenderState, pDevice, it->uiZFunc);

					TerrainPasses::DrawPass(it->pRenderPass, it->uiPassType, it->uiArg2, it->uiArg3, it->uiArg4);
					uiDrawn++;
				}
				return uiDrawn;
			};

			// Depth and `TOPPER_BIT`.
			{
				ForcedRenderStates kDepthOnly(pRenderState);
				kDepthOnly.Force(D3DRS_COLORWRITEENABLE, 0);
				uiCount = DrawPasses();
			}

			// The crop needs the depth with the toppers. The RESZ resolve of Depth Resolve draws a point, which must
			// not write stencil. It also binds the depth texture to stage 0 behind the cache.
			if (bCrop) {
				kStates.Update(D3DRS_STENCILWRITEMASK, 0);
				ResolveSceneDepth(apAccumulator, apTarget);
				TopperCrop::Crop(pDevice, apAccumulator->m_pkCamera, apDepthTexture);
				RenderStates::SyncTextureStages(pRenderState, pDevice, TEXTURE_STAGE_COUNT);
			}

			// Colour where `TOPPER_BIT` is set, and the topper's own `TERRAIN_BLEND_BIT`.
			kStates.Update(D3DRS_STENCILFUNC, D3DCMP_EQUAL);
			kStates.Update(D3DRS_STENCILWRITEMASK, StencilTags::TERRAIN_BLEND_BIT);
			DrawPasses();
		}

		// After the group locks are released, as in `RenderBatches`. The technique restores the states it changed.
		CdeclCall(END_TECHNIQUE);
		for (uint32_t i = 0; i < ARRAYSIZE(kInheritedStates); i++)
			pRenderState->SetRenderState(kInheritedStates[i], uiSavedInherited[i], 0, false);
		SetZFunc(pRenderState, pDevice, uiSavedZFunc);

		// After the forced states are restored. The resolve sets states of its own.
		ResolveSceneDepth(apAccumulator, apTarget);
		return uiCount;
	}

	// Returns the CPU time spent in depth resolves since the last call and resets it.
	double ConsumeResolveMilliseconds() {
		return std::exchange(dResolveMilliseconds, 0.0);
	}

	// Drops all recorded passes. Called once per frame before rendering, and after the replay.
	void Clear() {
		kRecordedPasses.clear();
	}

}
