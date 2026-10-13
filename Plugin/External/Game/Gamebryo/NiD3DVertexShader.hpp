#pragma once

#include "NiD3DShaderProgram.hpp"

NiSmartPointer(NiD3DVertexShader);

class NiD3DVertexShader : public NiD3DShaderProgram {
public:
	NiD3DVertexShader();
	virtual ~NiD3DVertexShader();

	virtual uint32_t					GetUsage() const;
	virtual void						SetUsage(uint32_t uiUsage);
	virtual LPDIRECT3DVERTEXSHADER9		GetShaderHandle() const;
	virtual void						SetShaderHandle(LPDIRECT3DVERTEXSHADER9 hShader);
	virtual LPDIRECT3DVERTEXSHADER9		GetVertexDeclaration() const;
	virtual void						SetVertexDeclaration(LPDIRECT3DVERTEXSHADER9 hShader);
	virtual bool						GetSoftwareVertexProcessing() const;
	virtual void						SetSoftwareVertexProcessing(bool bSoftwareVP);
	virtual void						DestroyRendererData();
	virtual void						RecreateRendererData();

	bool							m_bSoftwareVP;
	uint32_t						m_uiUsage;
	LPDIRECT3DVERTEXSHADER9			m_hShader;
	LPDIRECT3DVERTEXDECLARATION9	m_hDecl;

	NIRTTI_ADDRESS(0x126F9B4);
};

ASSERT_SIZE(NiD3DVertexShader, 0x3C);