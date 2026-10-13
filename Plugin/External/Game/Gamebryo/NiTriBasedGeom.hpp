#pragma once

#include "NiGeometry.hpp"
#include "NiTriBasedGeomData.hpp"

NiSmartPointer(NiTriBasedGeom);

class NiTriBasedGeom : public NiGeometry {
public:
	NiTriBasedGeom();
	virtual ~NiTriBasedGeom();

	virtual void GetModelTriangle(uint16_t usTriangle, NiPoint3*& pkP0, NiPoint3*& pkP1, NiPoint3*& pkP2);

	NIRTTI_ADDRESS(0x11F5B70);
};

ASSERT_SIZE(NiTriBasedGeom, 0xC4);
