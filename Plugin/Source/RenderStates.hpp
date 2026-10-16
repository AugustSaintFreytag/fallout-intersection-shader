#pragma once

#include "Gamebryo/NiDX9RenderState.hpp"

#include <vector>

// Offsets used by `NiDX9RenderState::SetRenderState` (GAME - 0xE88780) and `SetTexture` (GAME - 0xE88A20).
ASSERT_OFFSET(NiD3DRenderState, m_akRenderStateSettings, 0x120);
ASSERT_OFFSET(NiD3DRenderState, m_bSavedStates, 0x920);
ASSERT_OFFSET(NiD3DRenderState, m_apkTextureStageTextures, 0x10A0);

namespace RenderStates {

	void InstallHook();

	// A render state change the game requested while the state was locked.
	struct Request {
		bool bRequested;
		uint32_t uiValue;
		bool bSave;
	};

	void Track(D3DRENDERSTATETYPE aeState, bool abTrack);

	const Request& GetRequest(D3DRENDERSTATETYPE aeState);

	void SyncTextureStages(NiDX9RenderState* apRenderState, IDirect3DDevice9* apDevice, uint32_t auiStageCount);

}

// Forces render states at the `NiDX9RenderState` level for the lifetime of the object, then restores them.
//
// `NiDX9RenderState::SetRenderState` (GAME - 0xE88780) skips every change to a state whose `m_bSavedStates` entry
// is set. Its last argument sets that entry. Every render state the game sets goes through it, including the
// `BSRenderState` layer and the LOD Flicker Fix hook. Passes drawn in the meantime cannot override the forced values.
//
// On restore, a state gets the last value the game asked for while it was forced. A state the game did not touch
// gets its old value back. The old value alone would undo changes by the game: the opaque stage turns off the
// stencil test that the water reflection leaves on. Restoring that leftover would break every full-screen pass after it.
// Verified in RenderDoc capture 04, frames A and B.
//
// Never force `D3DRS_ZFUNC`. LOD Flicker Fix remaps its value on every set. Restoring it would remap it twice.
class ForcedRenderStates {
public:
	explicit ForcedRenderStates(NiDX9RenderState* apRenderState) : pRenderState(apRenderState) {}

	~ForcedRenderStates() {
		for (auto it = kSaved.rbegin(); it != kSaved.rend(); ++it) {
			uint32_t uiValue = it->uiValue;
			bool bSave = it->bSaved;

			if (it->bTracked) {
				const RenderStates::Request& rRequest = RenderStates::GetRequest(it->eState);
				if (rRequest.bRequested) {
					uiValue = rRequest.uiValue;
					bSave = rRequest.bSave;
				}
				RenderStates::Track(it->eState, false);
			}

			pRenderState->m_bSavedStates[it->eState] = false;
			pRenderState->SetRenderState(it->eState, uiValue, 0, false);
			pRenderState->m_bSavedStates[it->eState] = bSave;
		}
	}

	ForcedRenderStates(const ForcedRenderStates&) = delete;
	ForcedRenderStates& operator=(const ForcedRenderStates&) = delete;

	void Force(D3DRENDERSTATETYPE aeState, uint32_t auiValue) {
		// A state the game had already locked keeps its value on restore. Changes by the game were blocked anyway.
		const bool bSaved = pRenderState->m_bSavedStates[aeState];
		kSaved.push_back({ aeState, pRenderState->m_akRenderStateSettings[aeState].m_uiCurrValue, bSaved, !bSaved });

		pRenderState->m_bSavedStates[aeState] = false;
		pRenderState->SetRenderState(aeState, auiValue, 0, true);
		if (!bSaved)
			RenderStates::Track(aeState, true);
	}

	// Changes the value of a state this object already forces. The render state cache skips the device call if it is unchanged.
	void Update(D3DRENDERSTATETYPE aeState, uint32_t auiValue) {
		pRenderState->m_bSavedStates[aeState] = false;
		pRenderState->SetRenderState(aeState, auiValue, 0, true);
	}

private:
	struct SavedState {
		D3DRENDERSTATETYPE eState;
		uint32_t uiValue;
		bool bSaved;
		bool bTracked;
	};

	NiDX9RenderState* pRenderState;
	std::vector<SavedState> kSaved;
};
