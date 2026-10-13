#include "BSRenderState.hpp"

#ifdef GAME
BSRenderStateType* const BSRenderState::uiD3D2BSRS = reinterpret_cast<BSRenderStateType*>(0x11FFA30);
DWORD* const BSRenderState::iLocks = reinterpret_cast<DWORD*>(0x11FF9D8);
#else
BSRenderStateType* const BSRenderState::uiD3D2BSRS = reinterpret_cast<BSRenderStateType*>(0xF255D8);
DWORD* const BSRenderState::iLocks = reinterpret_cast<DWORD*>(0xF25580);
#endif

// GAME - 0xB97FA0
// GECK - 0x9312B0
void BSRenderState::SetAlphaBlendEnable(bool abEnable, BSRenderStateLock aeLock) {
#if GAME
	CdeclCall(0xB97FA0, abEnable, aeLock);
#else
	CdeclCall(0x9312B0, abEnable, aeLock);
#endif
}

// GAME - 0x714AE0
// GECK - 0x902DF0
void BSRenderState::RestoreAlphaBlendEnable(BSRenderStateLock aeLock) {
#if GAME
	CdeclCall(0x714AE0, aeLock);
#else
	CdeclCall(0x902DF0, aeLock);
#endif
}

// GAME - 0xB98380
// GECK - 0x931690
void BSRenderState::SetColorWriteEnable(uint32_t aMask, BSRenderStateLock aeLock) {
#if GAME
	CdeclCall(0xB98380, aMask, aeLock);
#else
	CdeclCall(0x931690, aMask, aeLock);
#endif
}

// GAME - 0x4ECED0
// GECK - 0x531C20
void BSRenderState::RestoreColorWriteEnable(BSRenderStateLock aeLock) {
#if GAME
	CdeclCall(0x4ECED0, aeLock);
#else
	CdeclCall(0x531C20, aeLock);
#endif
}

// GAME - 0xB97E30
// GECK - 0x931140
void BSRenderState::SetZWriteEnable(bool abEnable, BSRenderStateLock aeLock) {
#if GAME
	CdeclCall(0xB97E30, abEnable, aeLock);
#else
	CdeclCall(0x931140, abEnable, aeLock);
#endif
}

// GAME - 0x714A60
// GECK - 0x902DD0
void BSRenderState::RestoreZWriteEnable(BSRenderStateLock aeLock) {
#if GAME
	CdeclCall(0x714A60, aeLock);
#else
	CdeclCall(0x902DD0, aeLock);
#endif
}

// GAME - 0xB97DE0
// GECK - 0x9310F0
void BSRenderState::SetZEnable(D3DZBUFFERTYPE aeBufferType, BSRenderStateLock aeLock) {
#if GAME
	CdeclCall(0xB97DE0, aeBufferType, aeLock);
#else
	CdeclCall(0x9310F0, aeBufferType, aeLock);
#endif
}

// GAME - 0x714A40
void BSRenderState::RestoreZEnable(BSRenderStateLock aeLock) {
	iLocks[BSRS_Z_BUFFER] -= aeLock;
	SetZEnable(D3DZB_TRUE, BSRSL_NONE);
}

// GAME - 0xB97D90
// GECK - 0x9310A0
void BSRenderState::SetRenderState(D3DRENDERSTATETYPE aeRenderStateType, DWORD value, BSRenderStateLock aeLock) {
#if GAME
	CdeclCall(0xB97D90, aeRenderStateType, value, aeLock);
#else
	CdeclCall(0x9310A0, aeRenderStateType, value, aeLock);
#endif
}
