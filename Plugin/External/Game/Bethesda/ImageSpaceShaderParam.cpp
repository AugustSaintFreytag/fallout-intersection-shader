#include "ImageSpaceShaderParam.hpp"

// GAME - 0xB8A760
// GECk - 0x9398A0
void ImageSpaceShaderParam::SetVertexConstants(uint32_t auiIndex, float afVal1, float afVal2, float afVal3, float afVal4) {
#if GAME
	ThisCall(0xB8A760, this, auiIndex, afVal1, afVal2, afVal3, afVal4);
#else
	ThisCall(0x9398A0, this, auiIndex, afVal1, afVal2, afVal3, afVal4);
#endif
}

// GAME - 0xB8A790
// GECK - 0x9398D0
void ImageSpaceShaderParam::SetPixelConstants(uint32_t auiIndex, float afVal1, float afVal2, float afVal3, float afVal4) {
#if GAME
	ThisCall(0xB8A790, this, auiIndex, afVal1, afVal2, afVal3, afVal4);
#else
	ThisCall(0x9398D0, this, auiIndex, afVal1, afVal2, afVal3, afVal4);
#endif
}
