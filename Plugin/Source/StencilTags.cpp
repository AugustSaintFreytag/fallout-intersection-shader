#include "StencilTags.hpp"

#include "RenderStates.hpp"
#include "Settings.hpp"

#include "Bethesda/BSShaderAccumulator.hpp"
#include "Bethesda/BSShaderManager.hpp"
#include "Bethesda/BSShaderProperty.hpp"
#include "Gamebryo/NiCamera.hpp"
#include "Gamebryo/NiDX9Renderer.hpp"
#include "Gamebryo/NiNode.hpp"

#include <algorithm>
#include <memory>
#include <unordered_set>
#include <utility>

// Tags the pixels of statics that may blend with terrain in the stencil buffer while the opaque world is drawn.
//
// The opaque stage runs with stencil writes forced on. Every batch-rendered pass replaces the tag bit. Eligible
// statics set it. Everything else clears it where it draws in front, such as terrain, actors and other statics.
// The overlay pass only draws where the bit is set and clears the stencil buffer afterwards.
//
// A reference is eligible if its base form type is in the INI type list and its form ID is not blacklisted.
// Geometry whose bound lies entirely beyond `fMaxDistance` in view depth is not tagged. The overlay is fully faded
// out there, and every tagged pixel costs terrain shader runs in the overlay replay.
//
// Reference at `BSShaderAccumulator::FinishAccumulating_Standard_PreResolveDepth` (GAME - 0xB65AE0).
namespace StencilTags {

	ASSERT_OFFSET(BSShaderAccumulator, eRenderMode, 0x19C);

	// Calls of `FinishAccumulating_Standard_PreResolveDepth` (GAME - 0xB65AE0), cdecl `(BSShaderAccumulator*)`.
	// One is in `FinishAccumulatingPreResolveDepth` for the normal render mode (GAME - 0xB66520). The other is in the
	// function that runs both stages back to back (GAME - 0xB665A0). Depth Resolve replaces the calls of the second stage next to them.
	static constexpr uint32_t PRE_RESOLVE_CALL_STANDARD	= 0xB6653D;
	static constexpr uint32_t PRE_RESOLVE_CALL_COMBINED	= 0xB665A6;

	// Engine Layout
	// Reference at JIP LN NVSE and xNVSE.
	static constexpr uint32_t VTBL_BS_FADE_NODE			= 0x10A8F90;
	static constexpr uint32_t FADE_NODE_REFERENCE		= 0xCC;		// `TESObjectREFR*`, null for nodes without a reference
	static constexpr uint32_t REFR_BASE_FORM			= 0x20;		// `TESForm*`
	static constexpr uint32_t FORM_TYPE_ID				= 0x04;		// `uint8_t`
	static constexpr uint32_t FORM_REF_ID				= 0x0C;		// `uint32_t`
	static constexpr uint32_t DATA_HANDLER				= 0x11C3F2C;	// `DataHandler*`
	static constexpr uint32_t DATA_HANDLER_MOD_COUNT	= 0x218;	// `ModList::loadedModCount`
	static constexpr uint32_t DATA_HANDLER_MODS			= 0x21C;	// `ModList::loadedMods`, `ModInfo*[255]`
	static constexpr uint32_t MOD_INFO_NAME				= 0x20;		// `char[0x104]`
	static constexpr uint32_t MOD_INFO_INDEX			= 0x40C;	// `uint8_t`

	static constexpr uint32_t MAX_PARENT_DEPTH			= 64;

	typedef void(__cdecl* PreResolveDepthFn)(BSShaderAccumulator*);

	static CallDetour							kStandardDetour;
	static CallDetour							kCombinedDetour;
	static bool									bEnabled			= true;
	static bool									bTypeEligible[256]	= {};
	static std::unordered_set<uint32_t>			kBlacklist;
	static std::unique_ptr<ForcedRenderStates>	pTaggingStates;
	static const BSRenderedTexture*				pTaggedTarget		= nullptr;
	static uint32_t								uiTaggedStages		= 0;

	// Geometry and verdict of the last pass. Lit geometry draws one pass per light group, back to back.
	static const NiGeometry*					pLastGeometry		= nullptr;
	static bool									bLastEligible		= false;

	// Camera of the tagged stage, for the distance limit.
	static NiPoint3								kEye;
	static NiPoint3								kViewDirection;

	// Geometry verdicts since the last `ConsumeCounts`
	static uint32_t								uiTaggedGeometry	= 0;
	static uint32_t								uiDistantGeometry	= 0;

	template <typename T>
	static T Field(const void* apObject, uint32_t auiOffset) {
		return *reinterpret_cast<const T*>(reinterpret_cast<const uint8_t*>(apObject) + auiOffset);
	}

	// Returns the reference that owns the given scene graph object, from the first `BSFadeNode` up its parents that has one.
	// Returns null for terrain, grass and other objects without a reference.
	static const void* GetParentReference(const NiAVObject* apObject) {
		const NiAVObject* pNode = apObject;
		for (uint32_t uiDepth = 0; pNode && uiDepth < MAX_PARENT_DEPTH; uiDepth++) {
			if (Field<uint32_t>(pNode, 0) == VTBL_BS_FADE_NODE) {
				if (const void* pReference = Field<const void*>(pNode, FADE_NODE_REFERENCE))
					return pReference;
			}
			pNode = pNode->m_pkParent;
		}
		return nullptr;
	}

	// Returns whether any part of the geometry's bound lies within `fMaxDistance` in view depth of the tagged stage's camera.
	static bool IsInRange(const NiGeometry* apGeometry) {
		const NiBound* pBound = apGeometry->m_pWorldBound;
		if (!pBound)
			return true;

		const NiPoint3 kOffset(pBound->m_kCenter.x - kEye.x, pBound->m_kCenter.y - kEye.y, pBound->m_kCenter.z - kEye.z);
		const float fDepth = kOffset.x * kViewDirection.x + kOffset.y * kViewDirection.y + kOffset.z * kViewDirection.z;
		return fDepth - pBound->m_fRadius <= Settings::fMaxDistance;
	}

	static bool IsEligible(const NiGeometry* apGeometry) {
		const void* pReference = GetParentReference(apGeometry);
		if (!pReference)
			return false;

		const void* pBaseForm = Field<const void*>(pReference, REFR_BASE_FORM);
		if (!pBaseForm || !bTypeEligible[Field<uint8_t>(pBaseForm, FORM_TYPE_ID)])
			return false;

		return kBlacklist.empty() || !kBlacklist.contains(Field<uint32_t>(pBaseForm, FORM_REF_ID));
	}

	// Returns the load order index of a loaded plugin, or -1.
	static int32_t GetModIndex(const char* apName) {
		const void* pDataHandler = *reinterpret_cast<void**>(DATA_HANDLER);
		if (!pDataHandler)
			return -1;

		const uint32_t uiCount = std::min<uint32_t>(Field<uint32_t>(pDataHandler, DATA_HANDLER_MOD_COUNT), 0xFF);
		const void* const* ppMods = reinterpret_cast<const void* const*>(reinterpret_cast<const uint8_t*>(pDataHandler) + DATA_HANDLER_MODS);
		for (uint32_t i = 0; i < uiCount; i++) {
			const void* pMod = ppMods[i];
			if (pMod && !_stricmp(reinterpret_cast<const char*>(pMod) + MOD_INFO_NAME, apName))
				return Field<uint8_t>(pMod, MOD_INFO_INDEX);
		}
		return -1;
	}

	// Resolves the INI selection against the load order.
	// Call once the game data is loaded, at NVSE DeferredInit.
	void LoadSelection() {
		std::fill(std::begin(bTypeEligible), std::end(bTypeEligible), false);
		for (uint8_t ucType : Settings::kTerrainBlendTypes)
			bTypeEligible[ucType] = true;

		kBlacklist.clear();
		for (const Settings::FormEntry& rEntry : Settings::kTerrainBlendBlacklist) {
			const int32_t iModIndex = GetModIndex(rEntry.sPlugin.c_str());
			if (iModIndex < 0) {
				_MESSAGE("Terrain blend blacklist: %s is not loaded, skipped %s:%06X", rEntry.sPlugin.c_str(), rEntry.sPlugin.c_str(), rEntry.uiFormID);
				continue;
			}
			kBlacklist.insert((static_cast<uint32_t>(iModIndex) << 24) | rEntry.uiFormID);
		}

		_MESSAGE("Terrain blend selection: %u form types, %u blacklisted base forms", static_cast<uint32_t>(Settings::kTerrainBlendTypes.size()), static_cast<uint32_t>(kBlacklist.size()));
	}

	static void BeginTagging(const BSShaderAccumulator* apAccumulator) {
		// Gamebryo cameras look along their local x axis.
		const NiCamera* pCamera = apAccumulator->m_pkCamera;
		if (pCamera) {
			const NiMatrix3& kRotate = pCamera->m_kWorld.m_Rotate;
			kEye = pCamera->m_kWorld.m_Translate;
			kViewDirection = NiPoint3(kRotate.m_pEntry[0][0], kRotate.m_pEntry[1][0], kRotate.m_pEntry[2][0]);
		}
		else {
			kEye = NiPoint3(0.0f, 0.0f, 0.0f);
			kViewDirection = NiPoint3(0.0f, 0.0f, 0.0f);	// Every bound is in range.
		}

		pTaggingStates = std::make_unique<ForcedRenderStates>(NiDX9Renderer::GetSingleton()->m_pkRenderState);
		pTaggingStates->Force(D3DRS_STENCILENABLE, TRUE);
		pTaggingStates->Force(D3DRS_TWOSIDEDSTENCILMODE, FALSE);
		pTaggingStates->Force(D3DRS_STENCILFUNC, D3DCMP_ALWAYS);
		pTaggingStates->Force(D3DRS_STENCILPASS, D3DSTENCILOP_REPLACE);
		pTaggingStates->Force(D3DRS_STENCILFAIL, D3DSTENCILOP_KEEP);
		pTaggingStates->Force(D3DRS_STENCILZFAIL, D3DSTENCILOP_KEEP);
		pTaggingStates->Force(D3DRS_STENCILWRITEMASK, TERRAIN_BLEND_BIT);
		pTaggingStates->Force(D3DRS_STENCILREF, 0);

		pTaggedTarget	= BSShaderManager::GetCurrentRenderTarget();
		pLastGeometry	= nullptr;
		uiTaggedStages++;
	}

	static void EndTagging() {
		pTaggingStates.reset();
	}

	// Returns whether the given accumulator runs the opaque stage of the main scene.
	// These are the accumulators Depth Resolve runs its pre-water resolve for.
	static bool ShouldTag(const BSShaderAccumulator* apAccumulator) {
		return bEnabled && Settings::bEnabled
			&& apAccumulator->eRenderMode == BSShaderManager::BSSM_RENDER_NORMAL
			&& apAccumulator->bSetupWaterRefractionDepth
			&& BSShaderManager::GetCurrentRenderTarget();
	}

	// Runs the opaque stage, with tagging around it if the accumulator qualifies.
	static void RunPreResolveDepth(PreResolveDepthFn apOriginal, BSShaderAccumulator* apAccumulator) {
		const bool bTag = !pTaggingStates && ShouldTag(apAccumulator);
		if (bTag)
			BeginTagging(apAccumulator);

		apOriginal(apAccumulator);

		if (bTag)
			EndTagging();
	}

	static void __cdecl PreResolveDepthStandardHook(BSShaderAccumulator* apAccumulator) {
		RunPreResolveDepth(reinterpret_cast<PreResolveDepthFn>(kStandardDetour.GetOverwrittenAddr()), apAccumulator);
	}

	static void __cdecl PreResolveDepthCombinedHook(BSShaderAccumulator* apAccumulator) {
		RunPreResolveDepth(reinterpret_cast<PreResolveDepthFn>(kCombinedDetour.GetOverwrittenAddr()), apAccumulator);
	}

	// Hooks both calls of the opaque stage.
	void InstallHooks() {
		kStandardDetour.ReplaceCall(PRE_RESOLVE_CALL_STANDARD, PreResolveDepthStandardHook);
		kCombinedDetour.ReplaceCall(PRE_RESOLVE_CALL_COMBINED, PreResolveDepthCombinedHook);
		_MESSAGE("Stencil tagging installed (calls at %08X, %08X -> %08X, %08X)", PRE_RESOLVE_CALL_STANDARD, PRE_RESOLVE_CALL_COMBINED, kStandardDetour.GetOverwrittenAddr(), kCombinedDetour.GetOverwrittenAddr());
	}

	// Enables or disables tagging, for example when the overlay shaders are missing.
	void SetEnabled(bool abEnabled) {
		bEnabled = abEnabled;
	}

	// Sets the tag reference if the geometry of the given render pass is eligible.
	// Call before every batch-rendered pass draw. Returns true if `EndPass` has to be called after the draw.
	bool BeginPass(const void* apRenderPass) {
		if (!pTaggingStates || !apRenderPass)
			return false;

		const NiGeometry* pGeometry = static_cast<const BSShaderProperty::RenderPass*>(apRenderPass)->pGeometry;
		if (pGeometry != pLastGeometry) {
			pLastGeometry = pGeometry;
			bLastEligible = pGeometry && IsEligible(pGeometry);
			if (bLastEligible && !IsInRange(pGeometry)) {
				bLastEligible = false;
				uiDistantGeometry++;
			}
			else if (bLastEligible) {
				uiTaggedGeometry++;
			}
		}

		if (!bLastEligible)
			return false;

		pTaggingStates->Update(D3DRS_STENCILREF, TERRAIN_BLEND_BIT);
		return true;
	}

	// Resets the tag reference after a tagged pass.
	void EndPass() {
		// Draws outside the batch renderer (sky, LOD) must not inherit the tag.
		if (pTaggingStates)
			pTaggingStates->Update(D3DRS_STENCILREF, 0);
	}

	// Returns the number of opaque stages tagged since the last call and resets it. For diagnostics.
	uint32_t ConsumeTaggedStageCount() {
		return std::exchange(uiTaggedStages, 0);
	}

	// Returns the eligible geometry counted since the last call and resets the counts: tagged, and left untagged for
	// lying beyond `fMaxDistance`. Geometry drawn in several separate runs of passes counts once per run.
	GeometryCounts ConsumeGeometryCounts() {
		return { std::exchange(uiTaggedGeometry, 0), std::exchange(uiDistantGeometry, 0) };
	}

	// Returns whether the stencil buffer of the given target holds tags from this frame's opaque stage.
	bool HasTags(const BSRenderedTexture* apTarget) {
		return pTaggedTarget && pTaggedTarget == apTarget;
	}

	// Clears the stencil buffer of the bound target and forgets the tags.
	void Clear(IDirect3DDevice9* apDevice) {
		// Only tagging writes stencil between the start of the frame and the overlay pass.
		// Clearing the whole buffer restores what the water passes expect.
		// Verified in RenderDoc captures 02 and 03.
		if (pTaggedTarget)
			apDevice->Clear(0, nullptr, D3DCLEAR_STENCIL, 0, 1.0f, 0);
		pTaggedTarget = nullptr;
	}

}
