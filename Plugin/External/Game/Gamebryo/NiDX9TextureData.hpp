#pragma once

#include "NiPersistentSrcTextureRendererData.hpp"
#include "NiTexture.hpp"

class NiDX9Renderer;
class NiDX9SourceTextureData;
class NiDX9RenderedTextureData;
class NiDX9DynamicTextureData;

class NiDX9TextureData : public NiTexture::RendererData {
public:
	NiDX9TextureData();
	virtual ~NiDX9TextureData();

	virtual uint32_t					GetLevels() const;
	virtual LPDIRECT3DBASETEXTURE9		GetD3DTexture() const;
	virtual void						SetD3DTexture(LPDIRECT3DBASETEXTURE9 pkD3DTexture);
	virtual NiDX9RenderedTextureData*	GetAsRenderedTexture() const;
	virtual NiDX9SourceTextureData*		GetAsSourceTexture() const;
	virtual NiDX9DynamicTextureData*	GetAsDynamicTexture() const;
	virtual bool						InitializeFromD3DTexture(LPDIRECT3DBASETEXTURE9 pkD3DTexture);

	NiDX9Renderer*			m_pkRenderer;
	LPDIRECT3DBASETEXTURE9	m_pkD3DTexture;
	uint32_t				m_uiLevels;

	NiNewRTTI(NiDX9TextureData, NiTexture::RendererData);

	bool SaveTexture(const char* path, D3DXIMAGE_FILEFORMAT aeFormat);
};

ASSERT_SIZE(NiDX9TextureData, 0x6C);