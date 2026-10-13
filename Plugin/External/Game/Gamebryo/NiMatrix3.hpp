#pragma once

#include "NiPoint3.hpp"

class NiMatrix3 {
public:
	float	m_pEntry[3][3];

	NiMatrix3() {}
	NiMatrix3(float m00, float m10, float m20, float m01, float m11, float m21, float m02, float m12, float m22) noexcept
	{
		m_pEntry[0][0] = m00;
		m_pEntry[0][1] = m10;
		m_pEntry[0][2] = m20;
		m_pEntry[1][0] = m01;
		m_pEntry[1][1] = m11;
		m_pEntry[1][2] = m21;
		m_pEntry[2][0] = m02;
		m_pEntry[2][1] = m12;
		m_pEntry[2][2] = m22;
	}

	bool operator==(const NiMatrix3& mat) const noexcept;
	NiMatrix3 operator+(const NiMatrix3& mat) const noexcept;
	NiMatrix3 operator-(const NiMatrix3& mat) const noexcept;
	NiMatrix3 operator*(const NiMatrix3& mat) const noexcept;
	NiMatrix3 operator*(float fScalar) const noexcept;
	NiPoint3 operator*(const NiPoint3& pt) const noexcept;
	friend NiPoint3 operator*(const NiPoint3& pt, const NiMatrix3& mat) noexcept;

	NiMatrix3 operator/(float fScalar) const noexcept;
};


ASSERT_SIZE(NiMatrix3, 0x24);

typedef NiMatrix3 NiMatrix33;