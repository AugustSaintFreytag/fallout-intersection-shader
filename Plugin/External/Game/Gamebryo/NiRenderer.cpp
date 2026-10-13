#include "NiRenderer.hpp"

// GAME - 0x4E9BB0
// GECK - 0x531B50
void NiRenderer::SetCameraData(const NiCamera* apCamera) {
#if GAME
	ThisCall(0x4E9BB0, this, apCamera);
#else
	ThisCall(0x531B50, this, apCamera);
#endif
}