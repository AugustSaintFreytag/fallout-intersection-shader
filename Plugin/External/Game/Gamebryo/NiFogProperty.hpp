#pragma once

#include "NiProperty.hpp"
#include "NiColor.hpp"

NiSmartPointer(NiFogProperty);

class NiFogProperty : public NiProperty {
public:
	NiFogProperty();
	virtual ~NiFogProperty();

	Bitfield16	m_usFlags;
	float		m_fDepth;
	NiColor		m_kColor;

	CREATE_OBJECT(NiFogProperty, 0xA99FD0);
	NIRTTI_ADDRESS(0x11F5F34);
};

ASSERT_SIZE(NiFogProperty, 0x2C)