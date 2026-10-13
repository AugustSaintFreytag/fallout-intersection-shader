#pragma once

#include "d3d9types.h"
#include "Ni2DBuffer.hpp"
#include "NiTexture.hpp"

class NiRenderer;

NiSmartPointer(NiRenderedTexture);

class NiRenderedTexture : public NiTexture {
public:
	NiRenderedTexture();
	virtual ~NiRenderedTexture();

	virtual Ni2DBuffer* GetBuffer();

	Ni2DBufferPtr						m_spBuffer;
	uint32_t							uiBackgroundColor;
	int									unk38;
	Ni2DBuffer::MultiSamplePreference	eMSAAPref;
	bool								bIsRenderTarget;
	bool								bUseCustomFormat;
	D3DFORMAT							eCustomFormat;

	NIRTTI_ADDRESS(0x11F4AC4);
};

ASSERT_SIZE(NiRenderedTexture, 0x48);