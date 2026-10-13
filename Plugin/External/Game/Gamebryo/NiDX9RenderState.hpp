#pragma once

#include "d3d9caps.h"
#include "NiD3DRenderState.hpp"

class NiDX9RenderState : public NiD3DRenderState {
public:
	NiDX9RenderState();
	virtual ~NiDX9RenderState();

	bool							m_bDeclaration;
	uint32_t						m_uiCurrentFVF;
	uint32_t						m_uiPreviousFVF;
	LPDIRECT3DVERTEXDECLARATION9	m_hCurrentVertexDeclaration;
	LPDIRECT3DVERTEXDECLARATION9	m_hPreviousVertexDeclaration;
	bool							m_bUsingSoftwareVP;
	D3DCAPS9						m_kD3DCaps9;
};

ASSERT_SIZE(NiDX9RenderState, 0x1248);