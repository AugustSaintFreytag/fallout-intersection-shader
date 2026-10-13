#pragma once

#include "Gamebryo/NiObject.hpp"

class NiTexture;

class BSTextureSet : public NiObject {
public:
	BSTextureSet();
	virtual ~BSTextureSet();

	enum TextureType {
		BSTS_TEX_DIFFUSE	= 0,
		BSTS_TEX_NORMAL		= 1,
		BSTS_TEX_GLOW		= 2,
		BSTS_TEX_HEIGHT		= 3,
		BSTS_TEX_ENV		= 4,
		BSTS_TEX_ENV_MASK	= 5,
		BSTS_TEX_COUNT,
	};

	virtual const char*	GetTexturePath(TextureType aeTextureIndex) const;
	virtual void		GetTexture(TextureType aeTextureIndex, NiPointer<NiTexture>& aspTexture) const;
	virtual bool		SetTexturePath(TextureType aeTextureIndex, const char* apPath);

	NIRTTI_ADDRESS(0x1200444);
};

ASSERT_SIZE(BSTextureSet, 0x8);