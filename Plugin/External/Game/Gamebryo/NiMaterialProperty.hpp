#pragma once

#include "NiProperty.hpp"
#include "NiColor.hpp"

NiSmartPointer(NiMaterialProperty);

class NiMaterialProperty : public NiProperty {
public:
	NiMaterialProperty();
	virtual ~NiMaterialProperty();

	int32_t		m_iIndex;
	NiColor		m_spec;
	NiColor		m_emit;
	NiColor*	m_pExternalEmittance;
	float		m_fShine;
	float		m_fAlpha;
	float		m_fEmitMult;
	uint32_t	m_uiRevID;
	void*		m_pvRendererData;

	CREATE_OBJECT(NiMaterialProperty, 0xA756D0);
	NIRTTI_ADDRESS(0x11F4A5C);
};

ASSERT_SIZE(NiMaterialProperty, 0x4C)