#pragma once

#include "NiMemObject.hpp"
#include "NiD3DRSEntry.hpp"

class NiDX9RenderState;

class NiD3DRenderStateGroup : public NiMemObject {
public:
	bool			m_bRendererOwned;
	uint32_t		m_uiStateCount;
	NiD3DRSEntry*	m_pkRenderStates;
	uint32_t		m_uiSavedStateCount;
	NiD3DRSEntry*	m_pkSavedRenderStates;
};

ASSERT_SIZE(NiD3DRenderStateGroup, 0x14);