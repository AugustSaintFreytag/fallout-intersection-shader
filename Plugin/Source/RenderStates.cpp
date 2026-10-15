#include "RenderStates.hpp"

#include "Gamebryo/NiDX9Renderer.hpp"

namespace RenderStates {

	// Slot of `NiDX9RenderState::SetRenderState`. `BSRenderState::SetRenderState` calls [vtable + 0x68].
	static constexpr uint32_t SET_RENDER_STATE_SLOT	= 26;
	static constexpr uint32_t STATE_COUNT			= 256;

	typedef void(__thiscall* SetRenderStateFn)(NiDX9RenderState*, D3DRENDERSTATETYPE, uint32_t, int, bool);

	static SetRenderStateFn	pOriginal					= nullptr;
	static bool				bTracked[STATE_COUNT]		= {};
	static Request			kRequests[STATE_COUNT]		= {};

	// Records changes to tracked states that `NiDX9RenderState` is about to skip because they are locked.
	static void __fastcall SetRenderStateHook(NiDX9RenderState* apThis, void*, D3DRENDERSTATETYPE aeState, uint32_t auiValue, int aiUnused, bool abSave) {
		if (static_cast<uint32_t>(aeState) < STATE_COUNT && bTracked[aeState] && apThis->m_bSavedStates[aeState])
			kRequests[aeState] = { true, auiValue, abSave };

		pOriginal(apThis, aeState, auiValue, aiUnused, abSave);
	}

	// Hooks `NiDX9RenderState::SetRenderState` (vtable slot 26). `ForcedRenderStates` then sees the changes it blocks.
	// Call once the renderer exists and all plugins have installed their hooks, at NVSE DeferredInit.
	void InstallHook() {
		NiDX9RenderState* pRenderState = NiDX9Renderer::GetSingleton()->m_pkRenderState;
		void** ppVTable = *reinterpret_cast<void***>(pRenderState);

		// LOD Flicker Fix replaces this slot and calls the game function directly. Chain to the current slot entry.
		pOriginal = reinterpret_cast<SetRenderStateFn>(ppVTable[SET_RENDER_STATE_SLOT]);
		SafeWrite32(reinterpret_cast<SIZE_T>(&ppVTable[SET_RENDER_STATE_SLOT]), reinterpret_cast<SIZE_T>(&SetRenderStateHook));
		_MESSAGE("Render state hook installed (vtable %p, slot %u, previous %p)", ppVTable, SET_RENDER_STATE_SLOT, pOriginal);
	}

	// Starts or stops recording the changes requested for the given state while it is locked.
	void Track(D3DRENDERSTATETYPE aeState, bool abTrack) {
		if (static_cast<uint32_t>(aeState) >= STATE_COUNT)
			return;

		bTracked[aeState] = abTrack && pOriginal;
		kRequests[aeState] = {};
	}

	// Returns the last change requested for the given state since tracking started.
	// Binds the textures the render state cache holds for the first stages again on the device.
	//
	// Depth Resolve's RESZ resolve binds the depth texture to stage 0 behind the cache. A replayed pass whose texture
	// matches the cache entry would then draw with the depth texture. Clearing the cache entries instead unbinds the
	// textures on the device. A pass the game skips setting up, because it counts as set up already, then draws
	// without textures and turns black. With the device matching the cache, both cases draw with the right textures.
	// Not verified: which game check skips the setup.
	void SyncTextureStages(NiDX9RenderState* apRenderState, IDirect3DDevice9* apDevice, uint32_t auiStageCount) {
		for (uint32_t i = 0; i < auiStageCount; i++)
			apDevice->SetTexture(i, apRenderState->m_apkTextureStageTextures[i]);
	}

	const Request& GetRequest(D3DRENDERSTATETYPE aeState) {
		return kRequests[aeState];
	}

}
