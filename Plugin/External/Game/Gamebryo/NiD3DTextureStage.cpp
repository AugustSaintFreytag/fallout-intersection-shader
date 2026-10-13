#include "NiD3DTextureStage.hpp"

// GAME - 0xE7ECD0
// GAME - 0xC15520
void NiD3DTextureStage::ReturnStageToPool() {
#ifdef GAME
	ThisCall(0xE7ECD0, this);
#else
	ThisCall(0xC15520, this);
#endif
}

void NiD3DTextureStage::IncRefCount() {
	m_uiRefCount++;
}

void NiD3DTextureStage::DecRefCount() {
	m_uiRefCount--;
	if (m_uiRefCount == 0)
		ReturnStageToPool();
}
