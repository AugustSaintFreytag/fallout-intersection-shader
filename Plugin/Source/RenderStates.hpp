#pragma once

#include "Gamebryo/NiDX9RenderState.hpp"

#include <vector>

// Offsets used by NiDX9RenderState::SetRenderState (GAME - 0xE88780) and SetTexture (GAME - 0xE88A20).
ASSERT_OFFSET(NiD3DRenderState, m_akRenderStateSettings, 0x120);
ASSERT_OFFSET(NiD3DRenderState, m_bSavedStates, 0x920);
ASSERT_OFFSET(NiD3DRenderState, m_apkTextureStageTextures, 0x10A0);

// Forces render states at the NiDX9RenderState level for the lifetime of the object, then restores them.
// NiDX9RenderState::SetRenderState (GAME - 0xE88780) skips every change to a state whose m_bSavedStates
// entry is set, and its last argument sets that entry. Every render state the game sets goes through it,
// including Bethesda's BSRenderState layer and LOD Flicker Fix's hook, so the passes drawn meanwhile
// cannot override the forced values.
// Never force D3DRS_ZFUNC: LOD Flicker Fix remaps its value on every set, so restoring it would remap it twice.
class ForcedRenderStates {
public:
	explicit ForcedRenderStates(NiDX9RenderState* apRenderState) : pRenderState(apRenderState) {}

	~ForcedRenderStates() {
		for (auto it = kSaved.rbegin(); it != kSaved.rend(); ++it) {
			pRenderState->m_bSavedStates[it->eState] = false;
			pRenderState->SetRenderState(it->eState, it->uiValue, 0, false);
			pRenderState->m_bSavedStates[it->eState] = it->bSaved;
		}
	}

	ForcedRenderStates(const ForcedRenderStates&) = delete;
	ForcedRenderStates& operator=(const ForcedRenderStates&) = delete;

	void Force(D3DRENDERSTATETYPE aeState, uint32_t auiValue) {
		kSaved.push_back({ aeState, pRenderState->m_akRenderStateSettings[aeState].m_uiCurrValue, pRenderState->m_bSavedStates[aeState] });
		pRenderState->m_bSavedStates[aeState] = false;
		pRenderState->SetRenderState(aeState, auiValue, 0, true);
	}

	// Changes the value of a state this object already forces. The render state cache skips the device call if it is unchanged.
	void Update(D3DRENDERSTATETYPE aeState, uint32_t auiValue) {
		pRenderState->m_bSavedStates[aeState] = false;
		pRenderState->SetRenderState(aeState, auiValue, 0, true);
	}

private:
	struct SavedState {
		D3DRENDERSTATETYPE	eState;
		uint32_t			uiValue;
		bool				bSaved;
	};

	NiDX9RenderState*		pRenderState;
	std::vector<SavedState>	kSaved;
};
