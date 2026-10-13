#include "StencilTags.hpp"

#include "RenderStates.hpp"
#include "Settings.hpp"

#include "Bethesda/BSShaderAccumulator.hpp"
#include "Bethesda/BSShaderManager.hpp"
#include "Bethesda/BSShaderProperty.hpp"
#include "Gamebryo/NiDX9Renderer.hpp"
#include "Gamebryo/NiNode.hpp"

#include <algorithm>
#include <memory>
#include <unordered_set>

namespace StencilTags {
	ASSERT_OFFSET(BSShaderAccumulator, eRenderMode, 0x19C);

	// Calls of FinishAccumulating_Standard_PreResolveDepth (GAME - 0xB65AE0), cdecl (BSShaderAccumulator*): from
	// FinishAccumulatingPreResolveDepth for the normal render mode (GAME - 0xB66520), and from the function that
	// runs both stages back to back (GAME - 0xB665A0). Depth Resolve replaces the calls of the second stage next to them.
	static constexpr uint32_t PRE_RESOLVE_CALL_STANDARD	= 0xB6653D;
	static constexpr uint32_t PRE_RESOLVE_CALL_COMBINED	= 0xB665A6;

	// Engine layout, from JIP LN NVSE and xNVSE.
	static constexpr uint32_t VTBL_BS_FADE_NODE			= 0x10A8F90;
	static constexpr uint32_t FADE_NODE_REFERENCE		= 0xCC;		// TESObjectREFR*, null for nodes without a reference
	static constexpr uint32_t REFR_BASE_FORM			= 0x20;		// TESForm*
	static constexpr uint32_t FORM_TYPE_ID				= 0x04;		// uint8_t
	static constexpr uint32_t FORM_REF_ID				= 0x0C;		// uint32_t
	static constexpr uint32_t DATA_HANDLER				= 0x11C3F2C;	// DataHandler*
	static constexpr uint32_t DATA_HANDLER_MOD_COUNT	= 0x218;	// ModList::loadedModCount
	static constexpr uint32_t DATA_HANDLER_MODS			= 0x21C;	// ModList::loadedMods, ModInfo*[255]
	static constexpr uint32_t MOD_INFO_NAME				= 0x20;		// char[0x104]
	static constexpr uint32_t MOD_INFO_INDEX			= 0x40C;	// uint8_t

	static constexpr uint32_t MAX_PARENT_DEPTH			= 64;

	typedef void(__cdecl* PreResolveDepthFn)(BSShaderAccumulator*);

	static CallDetour							kStandardDetour;
	static CallDetour							kCombinedDetour;
	static bool									bEnabled			= true;
	static bool									bTypeEligible[256]	= {};
	static std::unordered_set<uint32_t>			kBlacklist;
	static std::unique_ptr<ForcedRenderStates>	pTaggingStates;
	static const BSRenderedTexture*				pTaggedTarget		= nullptr;

	// The last pass's geometry and verdict. Lit geometry draws one pass per light group, back to back.
	static const NiGeometry*					pLastGeometry		= nullptr;
	static bool									bLastEligible		= false;

	template <typename T>
	static T Field(const void* apObject, uint32_t auiOffset) {
		return *reinterpret_cast<const T*>(reinterpret_cast<const uint8_t*>(apObject) + auiOffset);
	}

	// Returns the reference that owns a scene graph object: the first BSFadeNode up its parents that has one.
	// Terrain, grass and other objects without a reference return null.
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

	static void BeginTagging() {
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
	}

	static void EndTagging() {
		pTaggingStates.reset();
	}

	// The opaque stage of the main scene: the same accumulators Depth Resolve runs its pre-water resolve for.
	static bool ShouldTag(const BSShaderAccumulator* apAccumulator) {
		return bEnabled && Settings::bEnabled
			&& apAccumulator->eRenderMode == BSShaderManager::BSSM_RENDER_NORMAL
			&& apAccumulator->bSetupWaterRefractionDepth
			&& BSShaderManager::GetCurrentRenderTarget();
	}

	static void RunPreResolveDepth(PreResolveDepthFn apOriginal, BSShaderAccumulator* apAccumulator) {
		const bool bTag = !pTaggingStates && ShouldTag(apAccumulator);
		if (bTag)
			BeginTagging();

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

	void InstallHooks() {
		kStandardDetour.ReplaceCall(PRE_RESOLVE_CALL_STANDARD, PreResolveDepthStandardHook);
		kCombinedDetour.ReplaceCall(PRE_RESOLVE_CALL_COMBINED, PreResolveDepthCombinedHook);
		_MESSAGE("Stencil tagging installed (calls at %08X, %08X -> %08X, %08X)", PRE_RESOLVE_CALL_STANDARD, PRE_RESOLVE_CALL_COMBINED, kStandardDetour.GetOverwrittenAddr(), kCombinedDetour.GetOverwrittenAddr());
	}

	void SetEnabled(bool abEnabled) {
		bEnabled = abEnabled;
	}

	bool BeginPass(const void* apRenderPass) {
		if (!pTaggingStates || !apRenderPass)
			return false;

		const NiGeometry* pGeometry = static_cast<const BSShaderProperty::RenderPass*>(apRenderPass)->pGeometry;
		if (pGeometry != pLastGeometry) {
			pLastGeometry = pGeometry;
			bLastEligible = pGeometry && IsEligible(pGeometry);
		}

		if (!bLastEligible)
			return false;

		pTaggingStates->Update(D3DRS_STENCILREF, TERRAIN_BLEND_BIT);
		return true;
	}

	void EndPass() {
		// Draws outside the batch renderer (sky, LOD) must not inherit the tag.
		if (pTaggingStates)
			pTaggingStates->Update(D3DRS_STENCILREF, 0);
	}

	bool HasTags(const BSRenderedTexture* apTarget) {
		return pTaggedTarget && pTaggedTarget == apTarget;
	}

	void Clear(IDirect3DDevice9* apDevice) {
		// Nothing writes stencil between the start of the frame and the overlay pass apart from us (RenderDoc
		// captures 02 and 03), so clearing the whole buffer restores what the water passes expect.
		if (pTaggedTarget)
			apDevice->Clear(0, nullptr, D3DCLEAR_STENCIL, 0, 1.0f, 0);
		pTaggedTarget = nullptr;
	}
}
