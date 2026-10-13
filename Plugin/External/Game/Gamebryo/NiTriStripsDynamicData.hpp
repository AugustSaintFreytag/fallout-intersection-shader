#pragma once

#include "NiTriStripsData.hpp"

NiSmartPointer(NiTriStripsDynamicData);

class NiTriStripsDynamicData : public NiTriStripsData {
public:
	NiTriStripsDynamicData();
	~NiTriStripsDynamicData();

	uint16_t m_usActiveVertices;
	uint16_t m_usActiveTriangles;

	CREATE_OBJECT(NiTriStripsDynamicData, 0xA923F0);
	NIRTTI_ADDRESS(0x11F5E8C);
};

ASSERT_SIZE(NiTriStripsDynamicData, 0x54)