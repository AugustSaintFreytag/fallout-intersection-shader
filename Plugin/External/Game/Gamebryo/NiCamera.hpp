#pragma once

#include "NiAVObject.hpp"
#include "NiRect.hpp"
#include "NiFrustum.hpp"

NiSmartPointer(NiCamera);

class NiRenderTargetGroup;

class NiCamera : public NiAVObject {
public:
	NiCamera();
	virtual ~NiCamera();

	float			m_aafWorldToCam[4][4];
	NiFrustum		m_kViewFrustum;
	float			m_fMinNearPlaneDist;
	float			m_fMaxFarNearRatio;
	NiRect<float>	m_kPort;
	float			m_fLODAdjust;

	CREATE_OBJECT(NiCamera, 0xA71430);
	NIRTTI_ADDRESS(0x11F4A18);
};

ASSERT_SIZE(NiCamera, 0x114);