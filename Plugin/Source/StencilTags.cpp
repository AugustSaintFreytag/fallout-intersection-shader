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
#include <string>
#include <unordered_set>
#include <utility>

// Tags the pixels of statics that may blend in the stencil buffer while the opaque world is drawn.
//
// The opaque stage runs with stencil writes forced on. Every batch-rendered pass replaces the tag bits. Statics that
// may blend with terrain set `TERRAIN_BLEND_BIT`, toppers for static-to-static blending set `TOPPER_BIT`. Everything
// else clears both where it draws in front, such as terrain, actors and other statics. Draws without depth writes
// leave the tags alone (`KeepsTags`). The overlay pass only draws where `TERRAIN_BLEND_BIT` is set. The stencil buffer
// is cleared after it.
//
// A reference may blend with terrain if its base form type is in the INI type list and its base form is not
// blacklisted. It is a topper if its base form is in the topper list. Both lists take editor IDs. Most editor IDs
// are only kept in memory with JohnnyGuitar NVSE.
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

	// `TESForm::GetFormByEditorID` (GAME - 0x483A00), cdecl `TESForm* (const char*)`.
	// JohnnyGuitar NVSE replaces it with a lookup in the editor ID map it fills for all form types it covers.
	// Reference at JohnnyGuitar NVSE, "JG/internal/JG/EditorIDRestoration.cpp".
	static constexpr uint32_t GET_FORM_BY_EDITOR_ID		= 0x483A00;

	static constexpr uint32_t MAX_PARENT_DEPTH			= 64;

	typedef void(__cdecl* PreResolveDepthFn)(BSShaderAccumulator*);

	static CallDetour							kStandardDetour;
	static CallDetour							kCombinedDetour;
	static bool									bEnabled			= true;
	static bool									bTypeEligible[256]	= {};
	static std::unordered_set<uint32_t>			kBlacklist;			// Base form IDs
	static std::unordered_set<uint32_t>			kToppers;			// Base form IDs
	static std::unique_ptr<ForcedRenderStates>	pTaggingStates;
	static const BSRenderedTexture*				pTaggedTarget		= nullptr;
	static uint32_t								uiTaggedStages		= 0;

	// Geometry and tag bits of the last pass. Lit geometry draws one pass per light group, back to back.
	static const NiGeometry*					pLastGeometry		= nullptr;
	static uint32_t								uiLastTags			= 0;

	// Camera of the tagged stage, for the distance limit.
	static NiPoint3								kEye;
	static NiPoint3								kViewDirection;

	// Untagged Geometry Log
	// Debug mode 3 logs why geometry gets no `TERRAIN_BLEND_BIT`, once per object.
	static constexpr uint32_t					MAX_UNTAGGED_LINES	= 200;
	static constexpr uint32_t					FORM_GET_EDITOR_ID	= 0x130;	// Vtable offset of `TESForm::GetEditorID`
	static std::unordered_set<std::string>		kLoggedUntagged;

	// Geometry verdicts since the last `ConsumeCounts`
	static uint32_t								uiTaggedGeometry	= 0;
	static uint32_t								uiTopperGeometry	= 0;
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

	// Returns the editor ID of a given form, or an empty string. IDs may not be available without JohnnyGuitar NVSE.
	static const char* GetEditorID(const void* apForm) {
		const uint32_t uiFunction = (*reinterpret_cast<const uint32_t* const*>(apForm))[FORM_GET_EDITOR_ID / 4];
		const char* pEditorID = ThisCall<const char*>(uiFunction, apForm);
		return pEditorID ? pEditorID : "";
	}

	// Logs why the given geometry gets no `TERRAIN_BLEND_BIT`, once per base form, or per scene root without a reference.
	// Debug mode 3 only.
	static void LogUntagged(const NiGeometry* apGeometry, const void* apBaseForm, const char* apReason) {
		if (Settings::eDebugMode != Settings::DEBUG_STENCIL_TAGS || kLoggedUntagged.size() >= MAX_UNTAGGED_LINES)
			return;

		if (apBaseForm) {
			char cKey[16];
			sprintf_s(cKey, "%08X", Field<uint32_t>(apBaseForm, FORM_REF_ID));
			if (!kLoggedUntagged.insert(cKey).second)
				return;
			_MESSAGE("Untagged: %s %s, form type %u: %s", cKey, GetEditorID(apBaseForm), Field<uint8_t>(apBaseForm, FORM_TYPE_ID), apReason);
			return;
		}

		const NiAVObject* pRoot = apGeometry;
		for (uint32_t uiDepth = 0; pRoot->m_pkParent && uiDepth < MAX_PARENT_DEPTH; uiDepth++)
			pRoot = pRoot->m_pkParent;
		const char* pRootName = pRoot->GetName() ? pRoot->GetName() : "";
		if (!kLoggedUntagged.insert(std::string("root ") + pRootName).second)
			return;
		_MESSAGE("Untagged: geometry \"%s\" under root \"%s\": %s", apGeometry->GetName() ? apGeometry->GetName() : "", pRootName, apReason);
	}

	// Returns the tag bits for the reference that owns the given geometry, 0 if it gets none.
	static uint32_t GetTags(const NiGeometry* apGeometry) {
		const void* pReference = GetParentReference(apGeometry);
		if (!pReference) {
			LogUntagged(apGeometry, nullptr, "no BSFadeNode with a reference above it");
			return 0;
		}

		const void* pBaseForm = Field<const void*>(pReference, REFR_BASE_FORM);
		if (!pBaseForm)
			return 0;

		const uint32_t uiFormID = Field<uint32_t>(pBaseForm, FORM_REF_ID);
		uint32_t uiTags = 0;
		if (!bTypeEligible[Field<uint8_t>(pBaseForm, FORM_TYPE_ID)])
			LogUntagged(apGeometry, pBaseForm, "form type not in sTerrainBlendTypes");
		else if (kBlacklist.contains(uiFormID))
			LogUntagged(apGeometry, pBaseForm, "blacklisted");
		else
			uiTags |= TERRAIN_BLEND_BIT;
		if (kToppers.contains(uiFormID))
			uiTags |= TOPPER_BIT;
		return uiTags;
	}

	// Resolves editor IDs to form IDs through the game's editor ID lookup. Logs every editor ID without a form.
	// Returns the form IDs.
	static std::unordered_set<uint32_t> ResolveEditorIDs(const std::vector<std::string>& arEditorIDs, const char* apListName) {
		std::unordered_set<uint32_t> kFormIDs;
		for (const std::string& rEditorID : arEditorIDs) {
			const void* pForm = CdeclCall<const void*>(GET_FORM_BY_EDITOR_ID, rEditorID.c_str());
			if (pForm)
				kFormIDs.insert(Field<uint32_t>(pForm, FORM_REF_ID));
			else
				_MESSAGE("%s: no form with editor ID %s, skipped", apListName, rEditorID.c_str());
		}
		return kFormIDs;
	}

	// Resolves the INI selection against the loaded forms.
	// Call once the game data is loaded, at NVSE DeferredInit.
	void LoadSelection() {
		std::fill(std::begin(bTypeEligible), std::end(bTypeEligible), false);
		for (uint8_t ucType : Settings::kBlendTypesTerrainToStatic)
			bTypeEligible[ucType] = true;

		kBlacklist	= ResolveEditorIDs(Settings::kTerrainToStaticBlacklist, "Terrain-to-static blacklist");
		kToppers	= ResolveEditorIDs(Settings::kStaticToStaticWhitelist, "Static-to-static whitelist");

		_MESSAGE("Terrain blend selection: %u form types, %u of %u blacklisted base forms found", static_cast<uint32_t>(Settings::kBlendTypesTerrainToStatic.size()),
			static_cast<uint32_t>(kBlacklist.size()), static_cast<uint32_t>(Settings::kTerrainToStaticBlacklist.size()));
		_MESSAGE("Static blend selection: %u of %u toppers found", static_cast<uint32_t>(kToppers.size()), static_cast<uint32_t>(Settings::kStaticToStaticWhitelist.size()));
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
		pTaggingStates->Force(D3DRS_STENCILWRITEMASK, TAG_BITS);
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

	// Sets the tag reference to the tag bits of the geometry of the given render pass, if it gets any.
	// Call before every batch-rendered pass draw. Returns the tag bits. `EndPass` has to be called after the draw
	// if they are not 0.
	uint32_t BeginPass(const void* apRenderPass) {
		if (!pTaggingStates || !apRenderPass)
			return 0;

		const NiGeometry* pGeometry = static_cast<const BSShaderProperty::RenderPass*>(apRenderPass)->pGeometry;
		if (pGeometry != pLastGeometry) {
			pLastGeometry = pGeometry;
			uiLastTags = pGeometry ? GetTags(pGeometry) : 0;
			if (uiLastTags && !IsInRange(pGeometry)) {
				uiLastTags = 0;
				uiDistantGeometry++;
			}
			else if (uiLastTags) {
				uiTaggedGeometry++;
				if (uiLastTags & TOPPER_BIT)
					uiTopperGeometry++;
			}
		}

		if (uiLastTags)
			pTaggingStates->Update(D3DRS_STENCILREF, uiLastTags);
		return uiLastTags;
	}

	// Resets the tag reference after a tagged pass.
	void EndPass() {
		// Draws outside the batch renderer (sky, LOD) must not inherit the tag.
		if (pTaggingStates)
			pTaggingStates->Update(D3DRS_STENCILREF, 0);
	}

	// Returns whether the coming draw has to leave the tags alone: a draw in the tagging window without depth writes.
	//
	// Such draws lie on surfaces drawn before them and hide nothing, such as decals and additive light passes. With the
	// stencil write mask of the window they would clear the tags of the surface under them. Which of them the game draws
	// changes with the camera.
	bool KeepsTags() {
		return pTaggingStates && !NiDX9Renderer::GetSingleton()->m_pkRenderState->m_akRenderStateSettings[D3DRS_ZWRITEENABLE].m_uiCurrValue;
	}

	// Returns the number of opaque stages tagged since the last call and resets it. For diagnostics.
	uint32_t ConsumeTaggedStageCount() {
		return std::exchange(uiTaggedStages, 0);
	}

	// Returns the eligible geometry counted since the last call and resets the counts: tagged, tagged as toppers, and
	// left untagged for lying beyond `fMaxDistance`. Geometry drawn in several separate runs of passes counts once per run.
	GeometryCounts ConsumeGeometryCounts() {
		return { std::exchange(uiTaggedGeometry, 0), std::exchange(uiTopperGeometry, 0), std::exchange(uiDistantGeometry, 0) };
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
