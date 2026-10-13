#include "BSRenderedTexture.hpp"

bool BSRenderedTexture::SaveTexture(const char* path, D3DXIMAGE_FILEFORMAT aeFormat) const {
	return static_cast<NiDX9TextureData*>(spRenderedTextures[0]->m_pkRendererData)->SaveTexture(path, aeFormat);
}

// GAME - 0xB6B610
// GECK - 0x9029E0
BSRenderedTexture* BSRenderedTexture::CreateTexture(const NiFixedString& arName, const uint32_t auiWidth, const uint32_t auiHeight, const NiTexture::FormatPrefs& arPrefs, Ni2DBuffer::MultiSamplePreference aeMSAAPref, bool abUseDepthStencil, NiDepthStencilBuffer* apDSBuffer, uint32_t a7, uint32_t auiBackgroundColor) {
#ifdef GAME
	return CdeclCall<BSRenderedTexture*>(0xB6B610, &arName, auiWidth, auiHeight, &arPrefs, aeMSAAPref, abUseDepthStencil, apDSBuffer, a7, auiBackgroundColor);
#else
	return CdeclCall<BSRenderedTexture*>(0x9029E0, &arName, auiWidth, auiHeight, &arPrefs, aeMSAAPref, abUseDepthStencil, apDSBuffer, a7, auiBackgroundColor);
#endif
}

BSRenderedTexture* BSRenderedTexture::Create(const uint32_t auiWidth, const uint32_t auiHeight) {
	NiTexture::FormatPrefs kPrefs(NiTexture::FormatPrefs::PIX_DEFAULT, NiTexture::FormatPrefs::ALPHA_DEFAULT, NiTexture::FormatPrefs::NO);
	return BSRenderedTexture::CreateTexture(0, auiWidth, auiHeight, kPrefs, Ni2DBuffer::MULTISAMPLE_NONE, 1, 0, 0, 0);
}

// GAME - 0x4BC320
// GECK - 0x48CBC0
NiRenderedTexture* BSRenderedTexture::GetTexture(const uint32_t uiIndex) const {
	if (this)
		return spRenderedTextures[uiIndex].m_pObject;
	else
		return nullptr;
}

// GAME - 0xB6B260
// GECK - 0x902710
NiRenderTargetGroup* BSRenderedTexture::GetRenderTargetGroup() const {
	NiRenderedTexture* pTexture = spRenderedTextures[0];
	if (pTexture && pTexture->IsExactKindOf<NiRenderedCubeMap>()) {
		return spRenderTargetGroups[static_cast<NiRenderedCubeMap*>(pTexture)->m_eFace];
	}
	else
		return spRenderTargetGroups[0];
}

// GAME - 0xB6B890
// GECK - 0x902C60
void BSRenderedTexture::Start(uint32_t auiClearMode, NiRenderTargetGroup* apGroup) {
#ifdef GAME
	CdeclCall(0xB6B890, auiClearMode, apGroup);
#else
	CdeclCall(0x902C60, auiClearMode, apGroup);
#endif
}

// GAME - 0xB6B8D0
// GECK - 0x902CA0
void BSRenderedTexture::StartOffscreen(uint32_t auiClearMode, NiRenderTargetGroup* apGroup) {
#ifdef GAME
	CdeclCall(0xB6B8D0, auiClearMode, apGroup);
#else
	CdeclCall(0x902CA0, auiClearMode, apGroup);
#endif
}

// GAME - 0xB6B7D0
// GECK - 0x902BA0
void BSRenderedTexture::Begin(NiRenderTargetGroup* apGroup, uint32_t uiClearMode) {
#ifdef GAME
	CdeclCall(0xB6B7D0, apGroup, uiClearMode);
#else
	CdeclCall(0x902BA0, apGroup, uiClearMode);
#endif
}

// GAME - 0xB6B730
// GECK - 0x902B00
void BSRenderedTexture::Stop() {
#ifdef GAME
	CdeclCall(0xB6B730);
#else
	CdeclCall(0x902B00);
#endif
}

// GAME - 0xB6B790
// GECK - 0x902B60
void BSRenderedTexture::StopOffscreen() {
#ifdef GAME
	CdeclCall(0xB6B790);
#else
	CdeclCall(0x902B60);
#endif
}
