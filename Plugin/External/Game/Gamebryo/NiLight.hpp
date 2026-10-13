#pragma once

#include "NiDynamicEffect.hpp"
#include "NiColor.hpp"

class TESObjectLIGH;

NiSmartPointer(NiLight);

class NiLight : public NiDynamicEffect {
public:
	NiLight();
	virtual ~NiLight();


	float	m_fDimmer;
	NiColor m_kAmb;
	NiColor m_kDiff;
	union { // Bethesda reuses red specular color for light radius, JIP adds a pointer to the light object
		NiColor	 m_kSpec;
		struct {
			float			m_fRadius;
			float			empty;
			TESObjectLIGH*	pLightForm;
		};
	};

	void* m_pvRendererData;

	NIRTTI_ADDRESS(0x11F4A28);
};

ASSERT_SIZE(NiLight, 0xF0)