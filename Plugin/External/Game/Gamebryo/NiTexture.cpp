#include "NiTexture.hpp"

// GAME - 0xA5F7B0
// GECK - 0x811000
void NiTexture::AddTextureToList() {
#if GAME
	ThisCall(0xA5F7B0, this);
#else
	ThisCall(0x811000, this);
#endif
}
