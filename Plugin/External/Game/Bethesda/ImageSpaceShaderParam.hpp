#pragma once

#include "ImageSpaceEffectParam.hpp"
#include "Gamebryo/NiTMap.hpp"
#include "Gamebryo/NiD3DShaderConstantMap.hpp"
#include "Gamebryo/NiRenderer.hpp"
#include "Gamebryo/NiColorA.hpp"

class ImageSpaceShaderParam : public ImageSpaceEffectParam {
public:
	ImageSpaceShaderParam();
	virtual ~ImageSpaceShaderParam();

	int													dword24;
	NiRenderer::ClearFlags								uiClearMode;
	NiColorA											kBackgroundColor;
	float*												pfVertexConstantGroup;
	uint32_t											uiVertexGroupSize;
	float*												pfPixelConstantGroup;
	uint32_t											uiPixelGroupSize;
	NiD3DShaderConstantMapPtr							spVertexConstantMap;
	NiD3DShaderConstantMapPtr							spPixelConstantMap;
	bool												bHasOffset[3];
	NiTObjectArray<NiPointer<NiTexture>>				kInputTextures;
	NiTPrimitiveArray<NiTexturingProperty::FilterMode>	kInputFilterModes;
	NiTMap<uint32_t, uint32_t>							kMap78;

	void SetVertexConstants(uint32_t auiIndex, float afVal1, float afVal2, float afVal3, float afVal4);
	void SetPixelConstants(uint32_t auiIndex, float afVal1, float afVal2, float afVal3, float afVal4);
};

ASSERT_SIZE(ImageSpaceShaderParam, 0x88);