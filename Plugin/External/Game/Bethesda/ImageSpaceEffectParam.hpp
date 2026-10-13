#pragma once

#include "Gamebryo/NiTArray.hpp"
#include "Gamebryo/NiTexturingProperty.hpp"

class ImageSpaceEffectParam {
public:
	ImageSpaceEffectParam();
	virtual ~ImageSpaceEffectParam();

	NiTPrimitiveArray<uint32_t>							eTextureType;
	NiTPrimitiveArray<NiTexturingProperty::FilterMode>	eFilterMode;
};

ASSERT_SIZE(ImageSpaceEffectParam, 0x24);