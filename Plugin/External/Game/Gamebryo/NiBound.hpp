#pragma once

#include "NiTransform.hpp"
#include "Bethesda/BSMemObject.hpp"

class NiCamera;
class NiPlane;

class NiBound {
public:
	NiBound() noexcept : m_kCenter(NiPoint3(0.0f,0.0f,0.0f)) , m_iRadius(0) {};
	NiBound(NiPoint3 akCenter, float afRadius) noexcept : m_kCenter(akCenter) , m_fRadius(afRadius) {};
	~NiBound() {};

	NiPoint3	m_kCenter;
	union {
		int32_t		m_iRadius;
		float		m_fRadius;
	};

	static constexpr float ms_fFuzzFactor	= 1e-04f;
	static constexpr float ms_fTolerance	= 1e-06f;

	bool operator==(const NiBound& arOther) const noexcept {
		return m_kCenter == arOther.m_kCenter && m_fRadius == arOther.m_fRadius;
	}

	BS_ALLOCATORS
};

ASSERT_SIZE(NiBound, 0x10);