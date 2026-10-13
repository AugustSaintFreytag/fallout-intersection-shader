#pragma once

#include "ImageSpaceEffect.hpp"

class DepthOfFieldTextures : public ImageSpaceEffect {
public:
	NiTPrimitiveArray<bool> kTextureStates;
};

class ImageSpaceEffectDepthOfField : public DepthOfFieldTextures {
public:
	float				float68;
	float				float6C;
	float				fStrength;
	DWORD				dword74;
	float				fUnk78;
	ImageSpaceTexture	kDOFMask;
	ImageSpaceTexture	kBuffers[3];
};

ASSERT_SIZE(ImageSpaceEffectDepthOfField, 0xBC);