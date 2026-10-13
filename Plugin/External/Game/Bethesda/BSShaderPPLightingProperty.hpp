#pragma once

#include "BSShaderLightingProperty.hpp"
#include "BSShaderTextureSet.hpp"
#include "Gamebryo/NiColorA.hpp"
#include "Gamebryo/NiPoint4.hpp"
#include "Gamebryo/NiTexturingProperty.hpp"

class NiAVObject;
class NiSourceTexture;
class BSShaderAccumulator;
class Lighting30ShaderProperty;
class NiAdditionalGeometryData;

NiSmartPointer(BSShaderPPLightingProperty);

class BSShaderPPLightingProperty : public BSShaderLightingProperty {
public:
	BSShaderPPLightingProperty();
	virtual ~BSShaderPPLightingProperty();

	virtual void							CopyTo3(Lighting30ShaderProperty* apTarget);
	virtual void							CopyProperties(BSShaderProperty* apTarget);
	virtual NiAdditionalGeometryData*		SetupGeometryData(NiGeometry* apGeometry);
	virtual D3DTEXTUREADDRESS				GetClampD3D() const;
	virtual void							SetClampD3D(D3DTEXTUREADDRESS aeClampMode);
	virtual NiTexturingProperty::ClampMode	GetClampNI() const;
	virtual void							SetClampNI(NiTexturingProperty::ClampMode aeClampMode);
	virtual uint16_t						GetLandscapeTextureCount();
	virtual int								GetLandPassCount();
	virtual void							SetTexture(BSTextureSet::TextureType aeTextureSlot, uint32_t auiTextureNumber, NiTexture* apTexture);
	virtual NiTexture*						GetTexture(BSTextureSet::TextureType aeTextureSlot, uint32_t auiTextureNumber);
	virtual void							LoadTextureSet(BSTextureSet* apTextureSet);
	virtual void							SetDiffuseTexture(uint32_t auiTextureNumber, NiTexture* apTexture);
	virtual void							SetNormalTexture(uint32_t auiTextureNumber, NiTexture* apTexture);
	virtual void							SetGlowTexture(uint32_t auiTextureNumber, NiTexture* apTexture);
	virtual void							SetSpecular();
	virtual float							GetSpecularFade();
	virtual float							GetEnvMapFade();
	virtual void							AddPassesOld(NiGeometry* apGeometry, uint32_t aeEnabledPasses, uint16_t* apusPassCount, uint32_t aeRenderMode, BSShaderAccumulator* apShaderAccum, bool abAddPass);
	virtual void							AddPasses(NiGeometry* apGeometry, uint32_t aeEnabledPasses, uint16_t* apusPassCount, uint32_t aeRenderMode, BSShaderAccumulator* apShaderAccum, bool abAddPass);

	struct SpecularExponents {
		uint8_t ucExponent[10];
	};

	struct SpecularAvailabilities {
		bool bHasSpecular[10];
	};

	NiSmartPointer(TextureEffectData);

	class TextureEffectData : public NiRefObject {
	public:
		TextureEffectData();
		~TextureEffectData();

		NiPointer<NiTexture>	spTexture08;
		NiColorA				kNewFillColor;
		NiColorA				kNewEdgeColor;
		NiColorA				kFillColor;
		NiColorA				kEdgeColor;
		float					fAnimU;
		float					fAnimV;
		float					fEdgeFalloff;
		float					fUnk58;
		D3DBLEND				eSourceBlend;
		D3DBLEND				eDestBlend;
		D3DBLENDOP				eBlendOperation;
		D3DCMPFUNC				eCompareFunc;
		NiPointer<NiTexture>	spTexEffect;
		DWORD					uiAlphaRef;
	};

	NiSmartPointer(TangentSpaceData);

	class TangentSpaceData : public NiRefObject {
	public:
		bool		unk8;
		NiPoint3*	pBinormals;
		NiPoint3*	pTangents;
	};

	struct LODTexParams {
		float fVal0;
		float fVal1;
		float fFadeTime;
		float fTextureScale;
	};

	uint32_t								unk07C;
	float									fMorphDistance;
	NiColorA								kHairTint;
	NiPoint4								kLandBlendParams;
	BSShaderTextureSetPtr					spTextureSet;
	uint16_t								usTextureCount;
	NiPointer<NiTexture>*					ppTextures[6];
	SpecularExponents*						pLandSpecularExponents;
	uint16_t								usLandPassCount;
	SpecularAvailabilities*					pLandSpecularStatus;
	TangentSpaceDataPtr						spTangentSpaceData;
	int8_t*									pcTexturePercents;
	NiTexturingProperty::ClampMode			eClampMode;
	TextureEffectDataPtr					spTexEffectData;
	float									fRefractionStrength;
	uint32_t								iRefractionFirePeriod;
	float									fParallaxMaxPasses;
	float									fParallaxScale;
	LODTexParams							kLODTextureParams;
	RenderPass*								pDepthPass;

	CREATE_OBJECT(BSShaderPPLightingProperty, 0xB68D50);
	NIRTTI_ADDRESS(0x11FA010);
};

ASSERT_SIZE(BSShaderPPLightingProperty, 0x104);
ASSERT_SIZE(BSShaderPPLightingProperty::TangentSpaceData, 0x14)
ASSERT_SIZE(BSShaderPPLightingProperty::TextureEffectData, 0x74)