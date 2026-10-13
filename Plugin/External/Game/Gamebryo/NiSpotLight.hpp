#pragma once

#include "NiPointLight.hpp"

NiSmartPointer(NiSpotLight);

class NiSpotLight : public NiPointLight {
public:
	NiSpotLight();
	~NiSpotLight();

	NiPoint3	m_kWorldDir;
	float		m_fOuterSpotAngle;
	float		m_fInnerSpotAngle;
	float		m_fSpotExponent;

	CREATE_OBJECT(NiSpotLight, 0xA94E20);
	NIRTTI_ADDRESS(0x11F5EC4);
};

#if !JIP_LIGHTS
ASSERT_SIZE(NiSpotLight, 0x114);
#endif