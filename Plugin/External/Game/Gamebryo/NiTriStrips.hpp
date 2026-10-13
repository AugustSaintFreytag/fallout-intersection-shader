#pragma once

#include "NiTriBasedGeom.hpp"
#include "NiTriStripsData.hpp"

NiSmartPointer(NiTriStrips);

class NiTriStrips : public NiTriBasedGeom {
public:
	NiTriStrips();
	virtual ~NiTriStrips();

	CREATE_OBJECT(NiTriStrips, 0xA71CE0);
	NIRTTI_ADDRESS(0x11F4A20);
};

ASSERT_SIZE(NiTriStrips, 0xC4)