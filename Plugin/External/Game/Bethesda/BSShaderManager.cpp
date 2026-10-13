#include "BSShaderManager.hpp"

BSTextureManager* BSShaderManager::GetTextureManager() {
#ifdef GAME
	return *reinterpret_cast<BSTextureManager**>(0x11F91A8);
#else
	return *reinterpret_cast<BSTextureManager**>(0xF23BF8);
#endif
}

BSRenderedTexture* BSShaderManager::GetCurrentRenderTarget() {
	return pCurrentRenderTarget;
}
