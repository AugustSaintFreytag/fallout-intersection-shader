#pragma once

#include "BSTextureSet.hpp"
#include "BSString.hpp"

NiSmartPointer(BSShaderTextureSet);

class BSShaderTextureSet : public BSTextureSet {
public:
	BSShaderTextureSet();
	~BSShaderTextureSet();

	BSString strTextures[6];

	CREATE_OBJECT(BSShaderTextureSet, 0xBA8A00);
	NIRTTI_ADDRESS(0x120044C);
};

ASSERT_SIZE(BSShaderTextureSet, 0x38);