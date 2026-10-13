#pragma once

#include "NiTArray.hpp"

class NiVBBlock;

class NiVBSet : public NiMemObject {
public:
	uint32_t						m_uiIndex;
	uint32_t						m_uiFVF;
	Bitfield32						m_uiFlags;
	uint32_t						m_uiBlockSize;
	LPDIRECT3DDEVICE9				m_pkD3DDevice;
	uint32_t						m_uiUsage;
	D3DPOOL							m_ePool;
	NiTPrimitiveArray<NiVBBlock*>	m_apkBlocks;
	NiVBBlock*						m_pkBlockList;
	uint32_t						m_uiFreeSpaceThreshold;
};

ASSERT_SIZE(NiVBSet, 0x34);