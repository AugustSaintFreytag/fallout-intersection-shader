#pragma once

#include "NiSourceTexture.hpp"
#include "NiTexture.hpp"

NiSmartPointer(NiSourceCubeMap);

class NiDX9Renderer;

class NiSourceCubeMap : public NiSourceTexture {
public:
	NiSourceCubeMap();
	~NiSourceCubeMap();

	enum FaceID : uint32_t {
		FACE_POS_X	= 0,
		FACE_NEG_X	= 1,
		FACE_POS_Y	= 2,
		FACE_NEG_Y	= 3,
		FACE_POS_Z	= 4,
		FACE_NEG_Z	= 5,
		FACE_NUM	= 6,
	};

	FaceID	m_eFace;

	CREATE_OBJECT(NiSourceCubeMap, 0xA60F30);
	NIRTTI_ADDRESS(0x11F4454);
};

ASSERT_SIZE(NiSourceCubeMap, 0x4C);