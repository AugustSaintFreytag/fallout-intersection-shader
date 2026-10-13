#include "NiPoint3.hpp"

NiPoint3 const NiPoint3::UNIT_X		= NiPoint3(1,0,0);
NiPoint3 const NiPoint3::UNIT_Y		= NiPoint3(0,1,0);
NiPoint3 const NiPoint3::UNIT_Z		= NiPoint3(0,0,1);
NiPoint3 const NiPoint3::UNIT_ALL	= NiPoint3(1,1,1);
NiPoint3 const NiPoint3::ZERO		= NiPoint3(0,0,0);

// GAME - 0x457990
// GECK - 0x40B3D0
float NiPoint3::Length() const {
#if USE_DXMATH
	return DirectX::XMVectorGetX(DirectX::XMVector3Length(DirectX::XMLoadNiPoint3(*this)));
#else
	return std::sqrt(x * x + y * y + z * z);
#endif
}

// GAME - 0x4A7290
float NiPoint3::SqrLength() const {
#if USE_DXMATH
	return DirectX::XMVectorGetX(DirectX::XMVector3LengthSq(DirectX::XMLoadNiPoint3(*this)));
#else
	return x * x + y * y + z * z;
#endif
}

// GAME - 0x4B6190
float NiPoint3::Dot(const NiPoint3& arOther) const {
#if USE_DXMATH
	return DirectX::XMVectorGetX(DirectX::XMVector3Dot(DirectX::XMLoadNiPoint3(*this), DirectX::XMLoadNiPoint3(arOther)));
#else
	return x * arOther.x + y * arOther.y + z * arOther.z;
#endif
}

// GAME - 0x4B3800
// GECK - 0x463390
NiPoint3 NiPoint3::Cross(const NiPoint3& arOther) const {
#if USE_DXMATH
	NiPoint3 kResult;
	DirectX::XMStoreNiPoint3(kResult, DirectX::XMVector3Cross(DirectX::XMLoadNiPoint3(*this), DirectX::XMLoadNiPoint3(arOther)));
	return kResult;
#else
	return NiPoint3(y * arOther.z - z * arOther.y, z * arOther.x - x * arOther.z, x * arOther.y - y * arOther.x);
#endif
}

// GAME - 0x4A0C10
// GECK - 0x40B400
void NiPoint3::Unitize() {
#if USE_DXMATH
	DirectX::XMStoreNiPoint3(*this, DirectX::XMVector3Normalize(DirectX::XMLoadNiPoint3(*this)));
#else
	float fLength = Length();

	if (fLength > 1e-06f) [[likely]] {
		float fRecip = 1.f / fLength;
		x *= fRecip;
		y *= fRecip;
		z *= fRecip;
	}
	else {
		*this = NiPoint3::ZERO;
	}
#endif
}

// GAME - 0x53D1A0
// GECK - 0x41F120
NiPoint3 NiPoint3::UnitCross(const NiPoint3& arOther) const {
#if USE_DXMATH
	NiPoint3 kResult;
	DirectX::XMVECTOR kThis = DirectX::XMLoadNiPoint3(*this);
	DirectX::XMVECTOR kOther = DirectX::XMLoadNiPoint3(arOther);

	DirectX::XMStoreNiPoint3(kResult, DirectX::XMVector3Normalize(DirectX::XMVector3Cross(kThis, kOther)));
	return kResult;
#else
	NiPoint3 cross(y * arOther.z - z * arOther.y, z * arOther.x - x * arOther.z, x * arOther.y - y * arOther.x);
	float fLength = cross.Length();
	if (fLength > 1e-06f)
		return cross / fLength;
	else
		return NiPoint3::ZERO;
#endif
}

float NiPoint3::Distance(const NiPoint3& arOther) const {
#if USE_DXMATH
	return DirectX::XMVectorGetX(DirectX::XMVector3Length(DirectX::XMVectorSubtract(DirectX::XMLoadNiPoint3(*this), DirectX::XMLoadNiPoint3(arOther))));
#else
	return (*this - arOther).Length();
#endif
}

// GAME - 0xA7EC90
// GECK - 0x82FFC0
void NiPoint3::PointsPlusEqualFloatTimesPoints(NiPoint3* __restrict apDst, float afMult, const NiPoint3* __restrict apSrc, uint32_t auiVerts) {
#if USE_DXMATH
	DirectX::XMVECTOR kMultiplier = DirectX::XMVectorReplicate(afMult);
	for (uint32_t i = 0; i < auiVerts; i++) {
		DirectX::XMVECTOR kResult = DirectX::XMVectorMultiplyAdd(kMultiplier, DirectX::XMLoadNiPoint3(apSrc[i]), DirectX::XMLoadNiPoint3(apDst[i]));
		DirectX::XMStoreNiPoint3(apDst[i], kResult);
	}
#else
	for (uint32_t i = 0; i < auiVerts; i++) {
		apDst[i] += afMult * apSrc[i];
	}
#endif
}

// GAME - 0x4B13C0
// GECK - 0x525C40
float NiPoint3::GetZAngleFromVector(const NiPoint3& arVector) {
#ifdef GAME
	return CdeclCall<float>(0x4B13C0, &arVector);
#else
	return CdeclCall<float>(0x525C40, &arVector);
#endif
}

// GAME - 0xA59BE0
// GECK - 0x80F580
void NiPoint3::LoadBinary(NiStream& arStream) {
#ifdef GAME
	ThisCall(0xA59BE0, this, &arStream);
#else
	ThisCall(0x80F580, this, &arStream);
#endif
}

// GAME - 0xA69780
// GECK - 0x80AD50
void NiPoint3::SaveBinary(NiStream& arStream) const {
#ifdef GAME
	ThisCall(0xA69780, this, &arStream);
#else
	ThisCall(0x80AD50, this, &arStream);
#endif
}
