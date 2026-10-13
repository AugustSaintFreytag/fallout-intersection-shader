#pragma once

#include "NiProperty.hpp"

NiSmartPointer(NiCullingProperty);

class  NiCullingProperty : public NiProperty {
public:
	Bitfield16 m_usFlags;

	CREATE_OBJECT(NiCullingProperty, 0xA8F580);
	NIRTTI_ADDRESS(0x11F5D20);
};

ASSERT_SIZE(NiCullingProperty, 0x1C)