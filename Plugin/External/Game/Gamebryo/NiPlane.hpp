#pragma once

#include "NiPoint3.hpp"

class NiPlane {
public:
	NiPlane() : m_kNormal(0.0f, 0.0f, 0.0f), m_fConstant(0.0f) {}
	NiPlane(const NiPoint3& arNormal, float afConstant) : m_kNormal(arNormal), m_fConstant(afConstant) {}
	NiPlane(const NiPoint3& arNormal, const NiPoint3& arPoint) : m_kNormal(arNormal), m_fConstant(arNormal.Dot(arPoint)) {}
	NiPlane(const NiPoint3& arPoint0, const NiPoint3& arPoint1, const NiPoint3& arPoint2) {
		NiPoint3 kDif1 = arPoint1 - arPoint0;
		NiPoint3 kDif2 = arPoint2 - arPoint1;
		m_kNormal = kDif1.UnitCross(kDif2);
		m_fConstant = m_kNormal * arPoint0;
	}

	NiPoint3	m_kNormal;
	float		m_fConstant;

	enum Sides {
		NO_SIDE			= 0,
		POSITIVE_SIDE	= 1,
		NEGATIVE_SIDE	= 2,
	};
};

ASSERT_SIZE(NiPlane, 0x10);