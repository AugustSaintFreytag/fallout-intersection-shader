#pragma once

#include "NiMatrix3.hpp"

bool NiMatrix3::operator==(const NiMatrix3& mat) const noexcept {
	return
		(m_pEntry[0][0] == mat.m_pEntry[0][0]) &&
		(m_pEntry[0][1] == mat.m_pEntry[0][1]) &&
		(m_pEntry[0][2] == mat.m_pEntry[0][2]) &&
		(m_pEntry[1][0] == mat.m_pEntry[1][0]) &&
		(m_pEntry[1][1] == mat.m_pEntry[1][1]) &&
		(m_pEntry[1][2] == mat.m_pEntry[1][2]) &&
		(m_pEntry[2][0] == mat.m_pEntry[2][0]) &&
		(m_pEntry[2][1] == mat.m_pEntry[2][1]) &&
		(m_pEntry[2][2] == mat.m_pEntry[2][2]);
}

NiMatrix3 NiMatrix3::operator+ (const NiMatrix3& mat) const noexcept {
	NiMatrix3 result = *this;
#if USE_DXMATH
	DirectX::XMStoreNiMatrix3(result, DirectX::XMLoadNiMatrix3(*this) + DirectX::XMLoadNiMatrix3(mat));
#else
	result.m_pEntry[0][0] += mat.m_pEntry[0][0];
	result.m_pEntry[0][1] += mat.m_pEntry[0][1];
	result.m_pEntry[0][2] += mat.m_pEntry[0][2];
	result.m_pEntry[1][0] += mat.m_pEntry[1][0];
	result.m_pEntry[1][1] += mat.m_pEntry[1][1];
	result.m_pEntry[1][2] += mat.m_pEntry[1][2];
	result.m_pEntry[2][0] += mat.m_pEntry[2][0];
	result.m_pEntry[2][1] += mat.m_pEntry[2][1];
	result.m_pEntry[2][2] += mat.m_pEntry[2][2];
#endif
	return result;
}

NiMatrix3 NiMatrix3::operator- (const NiMatrix3& mat) const noexcept {
	NiMatrix3 result = *this;
#if USE_DXMATH
	DirectX::XMStoreNiMatrix3(result, DirectX::XMLoadNiMatrix3(*this) - DirectX::XMLoadNiMatrix3(mat));
#else
	result.m_pEntry[0][0] -= mat.m_pEntry[0][0];
	result.m_pEntry[0][1] -= mat.m_pEntry[0][1];
	result.m_pEntry[0][2] -= mat.m_pEntry[0][2];
	result.m_pEntry[1][0] -= mat.m_pEntry[1][0];
	result.m_pEntry[1][1] -= mat.m_pEntry[1][1];
	result.m_pEntry[1][2] -= mat.m_pEntry[1][2];
	result.m_pEntry[2][0] -= mat.m_pEntry[2][0];
	result.m_pEntry[2][1] -= mat.m_pEntry[2][1];
	result.m_pEntry[2][2] -= mat.m_pEntry[2][2];
#endif
	return result;
}

NiMatrix3 NiMatrix3::operator* (const NiMatrix3& mat) const noexcept {
	NiMatrix3 result;
#if USE_DXMATH
	DirectX::XMStoreNiMatrix3(result, DirectX::XMLoadNiMatrix3(*this) * DirectX::XMLoadNiMatrix3(mat));
#else
	result.m_pEntry[0][0] =
		m_pEntry[0][0] * mat.m_pEntry[0][0] +
		m_pEntry[0][1] * mat.m_pEntry[1][0] +
		m_pEntry[0][2] * mat.m_pEntry[2][0];
	result.m_pEntry[1][0] =
		m_pEntry[1][0] * mat.m_pEntry[0][0] +
		m_pEntry[1][1] * mat.m_pEntry[1][0] +
		m_pEntry[1][2] * mat.m_pEntry[2][0];
	result.m_pEntry[2][0] =
		m_pEntry[2][0] * mat.m_pEntry[0][0] +
		m_pEntry[2][1] * mat.m_pEntry[1][0] +
		m_pEntry[2][2] * mat.m_pEntry[2][0];
	result.m_pEntry[0][1] =
		m_pEntry[0][0] * mat.m_pEntry[0][1] +
		m_pEntry[0][1] * mat.m_pEntry[1][1] +
		m_pEntry[0][2] * mat.m_pEntry[2][1];
	result.m_pEntry[1][1] =
		m_pEntry[1][0] * mat.m_pEntry[0][1] +
		m_pEntry[1][1] * mat.m_pEntry[1][1] +
		m_pEntry[1][2] * mat.m_pEntry[2][1];
	result.m_pEntry[2][1] =
		m_pEntry[2][0] * mat.m_pEntry[0][1] +
		m_pEntry[2][1] * mat.m_pEntry[1][1] +
		m_pEntry[2][2] * mat.m_pEntry[2][1];
	result.m_pEntry[0][2] =
		m_pEntry[0][0] * mat.m_pEntry[0][2] +
		m_pEntry[0][1] * mat.m_pEntry[1][2] +
		m_pEntry[0][2] * mat.m_pEntry[2][2];
	result.m_pEntry[1][2] =
		m_pEntry[1][0] * mat.m_pEntry[0][2] +
		m_pEntry[1][1] * mat.m_pEntry[1][2] +
		m_pEntry[1][2] * mat.m_pEntry[2][2];
	result.m_pEntry[2][2] =
		m_pEntry[2][0] * mat.m_pEntry[0][2] +
		m_pEntry[2][1] * mat.m_pEntry[1][2] +
		m_pEntry[2][2] * mat.m_pEntry[2][2];
#endif
	return result;
}

NiMatrix3 NiMatrix3::operator* (float fScalar) const noexcept {
#if USE_DXMATH
	NiMatrix3 result;
	DirectX::XMMATRIX kMatrix = DirectX::XMLoadNiMatrix3(*this);
	kMatrix.r[0] = DirectX::XMVectorScale(kMatrix.r[0], fScalar);
	kMatrix.r[1] = DirectX::XMVectorScale(kMatrix.r[1], fScalar);
	kMatrix.r[2] = DirectX::XMVectorScale(kMatrix.r[2], fScalar);
	DirectX::XMStoreNiMatrix3(result, kMatrix);
#else
	NiMatrix3 result = *this;
	result.m_pEntry[0][0] *= fScalar;
	result.m_pEntry[0][1] *= fScalar;
	result.m_pEntry[0][2] *= fScalar;
	result.m_pEntry[1][0] *= fScalar;
	result.m_pEntry[1][1] *= fScalar;
	result.m_pEntry[1][2] *= fScalar;
	result.m_pEntry[2][0] *= fScalar;
	result.m_pEntry[2][1] *= fScalar;
	result.m_pEntry[2][2] *= fScalar;
#endif
	return result;
}

NiPoint3 NiMatrix3::operator*(const NiPoint3& pt) const noexcept {
#if USE_DXMATH
	NiPoint3 result;
	DirectX::XMStoreNiPoint3(result, DirectX::XMMatrixTransform(DirectX::XMLoadNiMatrix3(*this), DirectX::XMLoadNiPoint3(pt)));
	return result;
#else
	return NiPoint3(
		m_pEntry[0][0] * pt.x + m_pEntry[0][1] * pt.y + m_pEntry[0][2] * pt.z,
		m_pEntry[1][0] * pt.x + m_pEntry[1][1] * pt.y + m_pEntry[1][2] * pt.z,
		m_pEntry[2][0] * pt.x + m_pEntry[2][1] * pt.y + m_pEntry[2][2] * pt.z);
#endif
}

NiPoint3 operator*(const NiPoint3& pt, const NiMatrix3& mat) noexcept {
#if USE_DXMATH
	NiPoint3 result;
	DirectX::XMVECTOR kPoint = DirectX::XMLoadNiPoint3(pt);
	DirectX::XMMATRIX kMatrix = DirectX::XMLoadNiMatrix3(mat);
	DirectX::XMVectorMultiply(kMatrix.r[0], DirectX::XMVectorSplatX(kPoint));
	DirectX::XMVectorMultiplyAdd(kMatrix.r[1], DirectX::XMVectorSplatY(kPoint), kMatrix.r[0]);
	DirectX::XMVectorMultiplyAdd(kMatrix.r[2], DirectX::XMVectorSplatZ(kPoint), kMatrix.r[1]);
	DirectX::XMStoreNiPoint3(result, kMatrix.r[2]);
	return result;
#else
	return NiPoint3(
		mat.m_pEntry[0][0] * pt.x + mat.m_pEntry[1][0] * pt.y + mat.m_pEntry[2][0] * pt.z,
		mat.m_pEntry[0][1] * pt.x + mat.m_pEntry[1][1] * pt.y + mat.m_pEntry[2][1] * pt.z,
		mat.m_pEntry[0][2] * pt.x + mat.m_pEntry[1][2] * pt.y + mat.m_pEntry[2][2] * pt.z);
#endif

}

NiMatrix3 NiMatrix3::operator/ (float fScalar) const noexcept {
#if USE_DXMATH
	NiMatrix3 result;
	DirectX::XMStoreNiMatrix3(result, DirectX::XMLoadNiMatrix3(*this) / fScalar);
#else
	NiMatrix3 result = *this;
	result.m_pEntry[0][0] /= fScalar;
	result.m_pEntry[0][1] /= fScalar;
	result.m_pEntry[0][2] /= fScalar;
	result.m_pEntry[1][0] /= fScalar;
	result.m_pEntry[1][1] /= fScalar;
	result.m_pEntry[1][2] /= fScalar;
	result.m_pEntry[2][0] /= fScalar;
	result.m_pEntry[2][1] /= fScalar;
	result.m_pEntry[2][2] /= fScalar;
#endif
	return result;
}
