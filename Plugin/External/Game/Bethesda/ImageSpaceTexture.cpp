#include "ImageSpaceTexture.hpp"

// GAME - 0xB4F530
// GECK - 0x8F8510
BSRenderedTexture* ImageSpaceTexture::GetDepthBuffer() {
#if GAME
	return CdeclCall<BSRenderedTexture*>(0xB4F530);
#else
	return CdeclCall<BSRenderedTexture*>(0x8F8510);
#endif
}

// GAME - 0xBA3780
// GECK - 0x937FE0
NiTexture* ImageSpaceTexture::GetTexture() const {
	if (bIsRenderedTexture)
		return static_cast<BSRenderedTexture*>(spTexture.m_pObject)->GetTexture(0);
	else
		return static_cast<NiTexture*>(spTexture.m_pObject);
}

// GAME - 0xBA39A0
// GECK - 0x938200
void ImageSpaceTexture::ReturnRenderedTexture() {
#if GAME
	ThisCall(0xBA39A0, this);
#else
	ThisCall(0x938200, this);
#endif
}

void ImageSpaceTexture::ClearTexture() {
	spTexture = nullptr;
	bIsRenderedTexture = false;
	iFilterMode = TEXTURE_FILTER_MODE_NEAREST;
	iClampMode = TEXTURE_ADDRESS_MODE_CLAMP_S_CLAMP_T;
}
