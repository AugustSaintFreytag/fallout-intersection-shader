#include "BSD3DTexture.hpp"
#include "Gamebryo/NiMemory.hpp"
#include "Gamebryo/NiDX9Renderer.hpp"


void* BSD3DTexture::vtable[41] = {};
void* BSD3DTextureData::vtable[45] = {};

BSD3DTexture* BSD3DTexture::CreateObject(IDirect3DTexture9* apD3DTexture) {
	BSD3DTexture* pTexture = NiNew<BSD3DTexture>();

	// Call NiObjectNET constructor and set NiTexture vtable.
#if GAME
	ThisCall(0xA5C200, pTexture);
	*(void**)pTexture = (void*)0x109B944;
#else
	ThisCall(0x8189C0, pTexture);
	*(void**)pTexture = (void*)0xDBAF54;
#endif

	if (!vtable[0]) [[unlikely]] {
		for (uint32_t i = 0; i < _countof(vtable); i++) {
			vtable[i] = ((void***)pTexture)[0][i];
		}

		ReplaceVTableEntry(vtable, 2, &BSD3DTexture::GetRTTIEx);
		ReplaceVTableEntry(vtable, 37, &BSD3DTexture::GetWidthEx);
		ReplaceVTableEntry(vtable, 38, &BSD3DTexture::GetHeightEx);
	}
	((DWORD*)pTexture)[0] = (DWORD)vtable;

	pTexture->m_kFormatPrefs.m_ePixelLayout = FormatPrefs::PIX_DEFAULT;
	pTexture->m_kFormatPrefs.m_eAlphaFmt = FormatPrefs::ALPHA_DEFAULT;
	pTexture->m_kFormatPrefs.m_eMipMapped = FormatPrefs::MIP_DEFAULT;
	pTexture->AddTextureToList();

	pTexture->m_pkRendererData = BSD3DTextureData::CreateObject(pTexture, apD3DTexture);

	return pTexture;
}

uint32_t BSD3DTexture::GetWidthEx() const {
	if (!m_pkRendererData)
		return 0;
	return m_pkRendererData->m_uiWidth;
}

uint32_t BSD3DTexture::GetHeightEx() const {
	if (!m_pkRendererData)
		return 0;
	return m_pkRendererData->m_uiHeight;
}

BSD3DTextureData* BSD3DTextureData::CreateObject(NiTexture* apTexture, IDirect3DTexture9* apD3DTexture) {
	BSD3DTextureData* pTextureData = NiNew<BSD3DTextureData>();

	NiDX9Renderer* pRenderer = NiDX9Renderer::GetSingleton();

	// Call NiDX9TextureData constructor.
#if GAME
	ThisCall(0xE8A260, pTextureData, apTexture, pRenderer);
#else
	ThisCall(0xC25070, pTextureData, apTexture, pRenderer);
#endif

	if (!vtable[0]) [[unlikely]] {
		for (uint32_t i = 0; i < _countof(vtable); i++) {
			vtable[i] = ((void***)pTextureData)[0][i];
		}

		ReplaceVTableEntry(vtable, 2, &BSD3DTextureData::GetRTTIEx);
	}
	((DWORD*)pTextureData)[0] = (DWORD)vtable;

	pTextureData->m_pkD3DTexture = apD3DTexture;
	pTextureData->InitializeFromD3DTexture(apD3DTexture);

	return pTextureData;
}
