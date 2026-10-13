#pragma once

#include "BSRenderedTexture.hpp"
#include "BSGraphics.hpp"

class ImageSpaceTexture {
public:
	ImageSpaceTexture();
	~ImageSpaceTexture();

	bool				byte0;
	bool				bIsRenderedTexture;
	bool				bIsBorrowed;
	NiObjectPtr			spTexture;
	uint32_t			iFilterMode;
	uint32_t			iClampMode;

	static BSRenderedTexture* GetDepthBuffer();

	NiTexture*	GetTexture() const;
	void		ReturnRenderedTexture();
	void		ClearTexture();
};

ASSERT_SIZE(ImageSpaceTexture, 0x10);
