#pragma once

#include "NiTriBasedGeomData.hpp"

NiSmartPointer(NiTriShapeData);

class NiTriShapeData : public NiTriBasedGeomData {
public:
	NiTriShapeData();
	virtual ~NiTriShapeData();

	class SharedNormalArray : public NiMemObject {
	public:
		uint16_t	m_usNumSharedNormals;
		uint16_t*	m_pusSharedNormalIndexArray;
	};

	class SNAMemBlock : public NiMemObject {
	public:
		uint16_t*		m_pusBlock;
		uint16_t*		m_pusFreeBlock;
		uint32_t		m_uiBlockSize;
		uint32_t		m_uiFreeBlockSize;
		SNAMemBlock*	m_pkNext;
	};

	uint32_t			m_uiTriListLength;
	uint16_t*			m_pusTriList;
	SharedNormalArray*	m_pkSharedNormals;
	uint16_t			m_usSharedNormalsArraySize;
	SNAMemBlock*		m_pkSNAMemoryBlocks;

	CREATE_OBJECT(NiTriShapeData, 0xA7B790);
	NIRTTI_ADDRESS(0x11F4A88);
};

ASSERT_SIZE(NiTriShapeData, 0x58)