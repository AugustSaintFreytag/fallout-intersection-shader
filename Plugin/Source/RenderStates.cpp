#include "RenderStates.hpp"

#include "Gamebryo/NiDX9Renderer.hpp"

namespace RenderStates {
	// NiDX9RenderState::SetRenderState is slot 26 (BSRenderState::SetRenderState calls [vtable + 0x68]).
	static constexpr uint32_t SET_RENDER_STATE_SLOT	= 26;
	static constexpr uint32_t STATE_COUNT			= 256;

	typedef void(__thiscall* SetRenderStateFn)(NiDX9RenderState*, D3DRENDERSTATETYPE, uint32_t, int, bool);

	static SetRenderStateFn	pOriginal					= nullptr;
	static bool				bTracked[STATE_COUNT]		= {};
	static Request			kRequests[STATE_COUNT]		= {};

	// Records changes to tracked states that NiDX9RenderState is about to skip because they are locked.
	static void __fastcall SetRenderStateHook(NiDX9RenderState* apThis, void*, D3DRENDERSTATETYPE aeState, uint32_t auiValue, int aiUnused, bool abSave) {
		if (static_cast<uint32_t>(aeState) < STATE_COUNT && bTracked[aeState] && apThis->m_bSavedStates[aeState])
			kRequests[aeState] = { true, auiValue, abSave };

		pOriginal(apThis, aeState, auiValue, aiUnused, abSave);
	}

	void InstallHook() {
		NiDX9RenderState* pRenderState = NiDX9Renderer::GetSingleton()->m_pkRenderState;
		void** ppVTable = *reinterpret_cast<void***>(pRenderState);

		// LOD Flicker Fix replaces this slot and calls the game function directly, so chain to whatever is there now.
		pOriginal = reinterpret_cast<SetRenderStateFn>(ppVTable[SET_RENDER_STATE_SLOT]);
		SafeWrite32(reinterpret_cast<SIZE_T>(&ppVTable[SET_RENDER_STATE_SLOT]), reinterpret_cast<SIZE_T>(&SetRenderStateHook));
		_MESSAGE("Render state hook installed (vtable %p, slot %u, previous %p)", ppVTable, SET_RENDER_STATE_SLOT, pOriginal);
	}

	void Track(D3DRENDERSTATETYPE aeState, bool abTrack) {
		if (static_cast<uint32_t>(aeState) >= STATE_COUNT)
			return;

		bTracked[aeState] = abTrack && pOriginal;
		kRequests[aeState] = {};
	}

	const Request& GetRequest(D3DRENDERSTATETYPE aeState) {
		return kRequests[aeState];
	}
}
