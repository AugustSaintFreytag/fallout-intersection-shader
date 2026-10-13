#pragma once

#include "NiTArray.hpp"
#include "NiVBSet.hpp"

class NiVBChip;

class NiVBBlock : public NiMemObject {
public:
	NiVBSet*						m_pkSet;
	uint32_t						m_uiIndex;
	LPDIRECT3DVERTEXBUFFER9			m_pkVB;
	uint32_t						m_uiSize;
	uint32_t						m_uiFVF;
	uint32_t						m_uiFlags;
	uint32_t						m_uiUsage;
	D3DPOOL							m_ePool;
	uint32_t						m_uiCurrOffset;
	uint32_t						m_uiFreeBytes;
	uint32_t						m_uiTotalFreeBytes;
	NiTPrimitiveArray<NiVBChip*>	m_apkChips;
	NiVBBlock*						m_pkNextBlock;
	NiVBBlock*						m_pkPrevBlock;

	uint32_t GetSize() const { return m_uiSize; }
};

ASSERT_SIZE(NiVBBlock, 0x44);