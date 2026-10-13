#pragma once

#include "ImageSpaceTexture.hpp"
#include "ImageSpaceShaderParam.hpp"
#include "ImageSpaceEffectParam.hpp"

class ImageSpaceManager;
class NiScreenGeometry;
class NiDX9Renderer;
class ImageSpaceShader;

class ImageSpaceEffect {
public:
	virtual ~ImageSpaceEffect();
	virtual void RenderShader(NiGeometry* apScreenShape, NiDX9Renderer* apRenderer, ImageSpaceEffectParam* apParam, bool abEndFrame);
	virtual void Setup(ImageSpaceManager* pISManager, ImageSpaceEffectParam* apParam);
	virtual void Shutdown();
	virtual void BorrowTextures(ImageSpaceEffectParam* apParam);
	virtual void ReturnTextures();
	virtual bool IsActive() const;
	virtual bool UpdateParams(ImageSpaceEffectParam* apParam);

	struct EffectInput {
		int32_t			  iTexIndex;
		int32_t			  iFilterMode;
	};


	bool										bIsActive;
	bool										bParamsChanged;
	NiTPrimitiveArray<ImageSpaceShader*>		kShaders;
	NiTPrimitiveArray<ImageSpaceShaderParam*>	kShaderParams;
	NiTPrimitiveArray<ImageSpaceTexture*>		kTextures;
	NiTPrimitiveArray<EffectInput*>				kShaderInputs;
	NiTPrimitiveArray<uint32_t*>				kShaderOutputs;
};

ASSERT_SIZE(ImageSpaceEffect, 0x58)