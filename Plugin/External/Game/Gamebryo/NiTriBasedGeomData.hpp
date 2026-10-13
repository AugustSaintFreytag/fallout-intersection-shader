#pragma once

#include "NiGeometryData.hpp"

NiSmartPointer(NiTriBasedGeomData);

class NiTriBasedGeomData : public NiGeometryData {
public:
	NiTriBasedGeomData();
	virtual ~NiTriBasedGeomData();

	virtual void		SetActiveTriangleCount(uint16_t ausCount);
	virtual uint16_t	GetActiveTriangleCount() const;
	virtual void		GetTriangleIndices(uint16_t i, uint16_t& i0, uint16_t& i1, uint16_t& i2) const;
	virtual void		GetStripData(uint16_t& usStrips, const uint16_t*& pusStripLengths, const uint16_t*& pusTriList, uint16_t& uiStripLengthSum) const;

	enum {
		TRIANGLE_INDEX_MASK = 0x10,
		TRIANGLE_COUNT_MASK = 0x20
	};

	uint16_t m_usTriangles;
	uint16_t m_usActiveTriangles;

	NIRTTI_ADDRESS(0x11F5B90);

	__forceinline uint16_t GetTriangleCount() const {
		return m_usTriangles;
	}
};

ASSERT_SIZE(NiTriBasedGeomData, 0x44);