#include "BSTextureManager.hpp"

// GAME - 0xB6E110
// GECK - 0x907440
BSRenderedTexture* BSTextureManager::BorrowRenderedTexture(NiDX9Renderer* apRenderer, eTextureType aeType, uint32_t auiFlags, uint32_t unk, uint32_t auiBackgroundColor) {
#if GAME
	return ThisCall<BSRenderedTexture*>(0xB6E110, this, apRenderer, aeType, auiFlags, unk, auiBackgroundColor);
#else
	return ThisCall<BSRenderedTexture*>(0x907440, this, apRenderer, aeType, auiFlags, unk, auiBackgroundColor);
#endif
}

// GAME - 0xB6DA10
// GECK - 0x906D40
void BSTextureManager::ReturnRenderedTexture(BSRenderedTexture* apRenderedTexture) {
#if GAME
	ThisCall(0xB6DA10, this, apRenderedTexture);
#else
	ThisCall(0x906D40, this, apRenderedTexture);
#endif
}
