#pragma once

#include "NiMemObject.hpp"

class NiVBDynamicSet : public NiMemObject {
public:
	uint32_t				m_uiIndex;
	uint32_t				m_uiFVF;
	uint32_t				m_uiBlockSize;
	LPDIRECT3DDEVICE9		m_pkD3DDevice;
	LPDIRECT3DVERTEXBUFFER9 m_pkVB;
	uint32_t				m_uiCurrOffset;
};

ASSERT_SIZE(NiVBDynamicSet, 0x18);