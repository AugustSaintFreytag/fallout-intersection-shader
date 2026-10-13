#pragma once

#include "NiTriBasedGeom.hpp"
#include "NiTriShapeData.hpp"

NiSmartPointer(NiTriShape);

class NiTriShape : public NiTriBasedGeom {
public:
	NiTriShape();
	virtual ~NiTriShape();

	CREATE_OBJECT(NiTriShape, 0xA744D0);
	NIRTTI_ADDRESS(0x11F4A40);
};

ASSERT_SIZE(NiTriShape, 0xC4);