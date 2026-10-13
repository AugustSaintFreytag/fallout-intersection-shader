#pragma once

#include "NiTriBasedGeomData.hpp"

NiSmartPointer(NiTriStripsData);

class NiTriStripsData : public NiTriBasedGeomData {
public:
	NiTriStripsData();
	virtual ~NiTriStripsData();

	uint16_t	m_usStrips;
	uint16_t*	m_pusStripLengths;
	uint16_t*	m_pusStripLists;

	CREATE_OBJECT(NiTriStripsData, 0xA75EC0);
	NIRTTI_ADDRESS(0x11F4A70);

	__forceinline uint16_t GetStripCount() const {
		return m_usStrips;
	}

	__forceinline uint16_t* GetStripLengths() const {
		return m_pusStripLengths;
	}

	__forceinline uint16_t* GetStripLists() const {
		return m_pusStripLists;
	}

	__forceinline uint16_t* GetIndices() const {
		return m_pusStripLists;
	}

	__forceinline uint32_t GetIndexCount() const {
		return m_pusStripLengths[0];
	}

	__forceinline uint32_t GetStripLengthSum() const {
		return m_usTriangles + 2 * m_usStrips;
	}
};

ASSERT_SIZE(NiTriStripsData, 0x50);