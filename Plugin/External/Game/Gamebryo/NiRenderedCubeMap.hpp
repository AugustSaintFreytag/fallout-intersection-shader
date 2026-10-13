#pragma once

#include "NiRenderedTexture.hpp"

class NiDX9Renderer;

NiSmartPointer(NiRenderedCubeMap);

class NiRenderedCubeMap : public NiRenderedTexture {
public:
	NiRenderedCubeMap();
	virtual ~NiRenderedCubeMap();

	enum FaceID {
		FACE_POS_X,
		FACE_NEG_X,
		FACE_POS_Y,
		FACE_NEG_Y,
		FACE_POS_Z,
		FACE_NEG_Z,
		FACE_NUM
	};

	FaceID			m_eFace;
	Ni2DBufferPtr	m_aspFaceBuffers[6];

	NIRTTI_ADDRESS(0x12708E4);

	void SetCurrentCubeFace(uint32_t eFace) {
		m_eFace = (FaceID)eFace;
		m_spBuffer = m_aspFaceBuffers[eFace];
	}

	void SetCubeFaceBuffer(uint32_t eFace, Ni2DBuffer* pkBuffer) {
		m_aspFaceBuffers[eFace] = pkBuffer;
	}

	Ni2DBuffer* GetFaceBuffer(uint32_t eFace) {
		return m_aspFaceBuffers[eFace];
	}
};

ASSERT_SIZE(NiRenderedCubeMap, 0x64);