#pragma once

#include <d3d9.h>
#include "NiVBDynamicSet.hpp"
#include "NiVBBlock.hpp"

class NiVBChip : public NiMemObject {
public:
	uint32_t				m_uiIndex;
	union {
		NiVBBlock* m_pkBlock;
		NiVBDynamicSet* m_pkDynamicSet;
	};
	LPDIRECT3DVERTEXBUFFER9 m_pkVB;
	uint32_t				m_uiOffset;
	Bitfield32				m_uiLockFlags;
	uint32_t				m_uiSize;
	NiVBChip* m_pkNextChip;
	NiVBChip* m_pkPrevChip;
};

ASSERT_SIZE(NiVBChip, 0x020);