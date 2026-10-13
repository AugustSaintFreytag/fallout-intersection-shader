#pragma once

#include "NiVBChip.hpp"
#include "NiShaderDeclaration.hpp"

class NiGeometryGroup;

class NiGeometryBufferData : public NiMemObject {
public:
	NiGeometryBufferData();

	virtual				~NiGeometryBufferData();
	virtual bool		ContainsVertexData(NiShaderDeclaration::ShaderParameter aeParameter);

	Bitfield32					m_uiFlags;
	NiGeometryGroup*			m_pkGeometryGroup;
	uint32_t					m_uiFVF;
	void*						m_hDeclaration;
	bool						m_bSoftwareVP;
	uint32_t					m_uiVertCount;
	uint32_t					m_uiMaxVertCount;
	uint32_t					m_uiStreamCount;
	uint32_t*					m_puiVertexStride;
	NiVBChip**					m_ppkVBChip;
	uint32_t					m_uiIndexCount;
	uint32_t					m_uiIBSize;
	LPDIRECT3DINDEXBUFFER9		m_pkIB;
	uint32_t					m_uiBaseVertexIndex;
	D3DPRIMITIVETYPE			m_eType;
	uint32_t					m_uiTriCount;
	uint32_t					m_uiMaxTriCount;
	uint32_t					m_uiNumArrays;
	const uint16_t*				m_pusArrayLengths;
	const uint16_t*				m_pusIndexArray;

	void SetStreamCount(uint32_t auiStreamCount);
};

ASSERT_SIZE(NiGeometryBufferData, 0x54);