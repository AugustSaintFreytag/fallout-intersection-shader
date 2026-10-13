#pragma once

#include "NiObject.hpp"
#include "NiPixelFormat.hpp"
#include "NiRect.hpp"

NiSmartPointer(Ni2DBuffer);

class Ni2DBuffer : public NiObject {
public:
	Ni2DBuffer();
	virtual ~Ni2DBuffer();

	enum CopyFilterPreference {
		COPY_FILTER_NONE	= 0,
		COPY_FILTER_POINT	= 1,
		COPY_FILTER_LINEAR	= 2,
		COPY_FILTER_MAX		= 3,
	};

	enum MultiSamplePreference {
		MULTISAMPLE_NONE	= 0,
		MULTISAMPLE_2		= 1,
		MULTISAMPLE_4		= 2,
		MULTISAMPLE_8		= 3,
	};

	virtual void FastCopy(Ni2DBuffer* pkDest, const NiRect<uint32_t>* pkSrcRect, uint32_t uiDestX, uint32_t uiDestY);
	virtual void Copy(Ni2DBuffer* pkDest, const NiRect<uint32_t>* pkSrcRect, const NiRect<uint32_t>* pkDestRect, Ni2DBuffer::CopyFilterPreference ePref);
	virtual void CreateRendererData();

	class RendererData : public NiRefObject {
	public:
		RendererData();
		virtual ~RendererData();

		Ni2DBuffer*				m_pkBuffer;
		const NiPixelFormat*	m_pkPixelFormat;
		int16_t					m_eMSAAPref;
	};

	typedef NiPointer<RendererData> RendererDataPtr;

	class NiDX9TextureBufferData : RendererData {
	public:
		IDirect3DSurface9*	m_pkD3DSurface;
	};

	uint32_t			m_uiWidth;
	uint32_t			m_uiHeight;
	RendererDataPtr		m_spRendererData;

	NIRTTI_ADDRESS(0x11F5D00);

	uint32_t GetWidth() const { return m_uiWidth; }
	uint32_t GetHeight() const { return m_uiHeight; }

	NiDX9TextureBufferData* GetDX9RendererData() const {
		return reinterpret_cast<NiDX9TextureBufferData*>(m_spRendererData.m_pObject);
	}
};

ASSERT_SIZE(Ni2DBuffer, 0x014);