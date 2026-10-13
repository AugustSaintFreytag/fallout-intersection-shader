#pragma once

#include "Gamebryo/NiShadeProperty.hpp"

class BSShaderAccumulator;
class NiSourceTexture;
class Lighting30ShaderProperty;
class ShadowSceneLight;
class BGSTextureUseMap;

NiSmartPointer(BSShaderProperty);

enum Flags {
	BSSP_SPECULAR						= 0,
	BSSP_SKINNED						= 1,
	BSSP_LOW_DETAIL						= 2,
	BSSP_VERTEX_ALPHA					= 3,
	BSSP_MOTION_BLUR					= 4,
	BSSP_SINGLE_PASS					= 5,
	BSSP_EMPTY							= 6,
	BSSP_ENVIRONMENT_MAPPING			= 7,
	BSSP_ALPHA_TEXTURE					= 8,
	BSSP_ZPREPASS						= 9,
	BSSP_FACEGEN						= 10,
	BSSP_PARALLAX						= 11,
	BSSP_MODEL_SPACE_NORMALS			= 12,
	BSSP_NON_PROJECTIVE_SHADOWS			= 13,
	BSSP_LANDSCAPE						= 14,
	BSSP_REFRACTION						= 15,
	BSSP_FIRE_REFRACTION				= 16,
	BSSP_EYE_ENVIRONMENT_MAPPING		= 17,
	BSSP_HAIR							= 18,
	BSSP_DYNAMIC_ALPHA					= 19,
	BSSP_LOCAL_MAP_HIDE_SECRET			= 20,
	BSSP_WINDOW_ENVIRONMENT_MAPPING		= 21,
	BSSP_TREE_BILLBOARD					= 22,
	BSSP_SHADOW_FRUSTUM					= 23,
	BSSP_MULTIPLE_TEXTURES				= 24,
	BSSP_REMAPPABLE_TEXTURES			= 25,
	BSSP_DECAL							= 26,
	BSSP_DYNAMIC_DECAL					= 27,
	BSSP_PARALLAX_OCCLUSION				= 28,
	BSSP_EXTERNAL_EMITTANCE				= 29,
	BSSP_SHADOW_MAP						= 30,
	BSSP_ZBUFFER_TEST					= 31,
	BSSP_ZBUFFER_WRITE					= 32,
	BSSP_LOD_LANDSCAPE					= 33,
	BSSP_LOD_BUILDING					= 34,
	BSSP_NO_FADE						= 35,
	BSSP_REFRACTION_TINT				= 36,
	BSSP_VERTEX_COLORS					= 37,
	BSSP_1ST_PERSON						= 38,
	BSSP_1ST_LIGHT_IS_POINTLIGHT		= 39,
	BSSP_2ND_LIGHT						= 40,
	BSSP_3RD_LIGHT						= 41,
	BSSP_VERTEX_LIGHTING				= 42,
	BSSP_UNIFORM_SCALE					= 43,
	BSSP_FIT_SLOPE						= 44,
	BSSP_BILLBOARD_ENV_FADE				= 45,
	BSSP_NO_LOD_LAND_BLEND				= 46,
	BSSP_ENVMAP_LIGHT_FADE				= 47,
	BSSP_WIRE_FRAME						= 48,
	BSSP_VATS_SELECTION					= 49,
	BSSP_SHOW_IN_LOCAL_MAP				= 50,
	BSSP_PREMULT_ALPHA					= 51,
	BSSP_SKIP_NORMAL_MAPS				= 52,
	BSSP_ALPHA_DECAL					= 53,
	BSSP_NO_TRANSPARENCY_MULTISAMPLING	= 54,
	BSSP_STINGER_PROP					= 55,
	BSSP_UNK3							= 56,
	BSSP_UNK4							= 57,
	BSSP_UNK5							= 58,
	BSSP_UNK6							= 59,
	BSSP_UNK7							= 60,
	BSSP_UNK8							= 61,
	BSSP_UNK9							= 62,
	BSSP_REALTIME_CUBEMAP				= 63,
	BSSP_MAX_FLAGS						= 64
};


class BSShaderProperty : public NiShadeProperty {
public:
	BSShaderProperty();
	virtual ~BSShaderProperty();

	class RenderPass {
	public:
		enum AccumMode : uint8_t {
			ACCUM_NONE				= 0,
			ACCUM_ALPHA_BLEND		= 1,
			ACCUM_UNK_2				= 2,
			ACCUM_PARTICLE			= 3,
			ACCUM_DECAL_SINGLE		= 4,
			ACCUM_DYN_DECAL_SINGLE	= 5,
			ACCUM_REFRACTION		= 6,
			ACCUM_REFRACTION_CLEAR	= 7,
			ACCUM_UNK_8				= 8,
			ACCUM_UNK_9				= 9,
			ACCUM_LOD				= 10,
			ACCUM_UNK_11			= 11,
			ACCUM_UNK_12			= 12,
			ACCUM_UNK_13			= 13,
			ACCUM_ALPHA_FADE		= 14,
		};


		RenderPass();
		~RenderPass();

		NiGeometry*			pGeometry;
		uint16_t			usPassEnum;
		AccumMode			eAccumulationHint;
		bool				bIsFirst;
		bool				bNoFog;
		uint8_t				ucNumLights;
		uint8_t				ucMaxNumLights;
		uint8_t				cCurrLandTexture;
		ShadowSceneLight**	ppSceneLights;
	};

	class RenderPassArray : public NiTObjectArray<RenderPass*> {
	public:
		RenderPassArray();
		virtual ~RenderPassArray();

		uint32_t uiPassCount;
	};

	virtual void						CopyTo(BSShaderProperty* apTarget);
	virtual void						CopyData(BSShaderProperty* apTarget);
	virtual void						SetupGeometry(NiGeometry* apGeometry);
	virtual RenderPassArray*			GetRenderPasses(const NiGeometry* apGeometry, const uint32_t auiEnabledPasses, uint16_t* apusPassCount, const uint32_t aeRenderMode, BSShaderAccumulator* apAccumulator, bool abAddPass);
	virtual uint16_t					GetNumberofPasses(NiGeometry* apGeometry);
	virtual RenderPassArray*			GetSIBlockRenderPasses() const;
	virtual RenderPass*					GetDepthPass(NiGeometry* apGeometry);
	virtual BSShaderProperty*			PickShader(NiGeometry* apGeometry, uint32_t unk0 = 0, uint32_t unk2 = 1);
	virtual NiSourceTexture*			GetDiffuseTexture() const;
	virtual RenderPassArray*			GetWaterDepthPass(NiGeometry* apGeometry);
	virtual void						CountTextures(void* apCountFunc, BGSTextureUseMap* apTexMap) const;
	virtual void						PrecacheTextures() const;

	Bitfield32			ulFlags[2];
	float				fAlpha;
	float				fFadeAlpha;
	float				fEnvMapScale;
	float				fLODFade;
	int32_t				iLastRenderPassState;
	RenderPassArray*	pRenderPassArray;
	RenderPassArray*	pRenderPassArray_depthMap;
	RenderPassArray*	pRenderPassArray_constAlpha;
	RenderPassArray*	pRenderPassArray_localMap;
	RenderPassArray*	pRenderPassArray_siblock;
	RenderPassArray*	pRenderPassArray_waterDepth;
	RenderPassArray*	pRenderPassArray_silhouette;
	uint32_t			uiShaderIndex;
	float				fDepthBias;

	CREATE_OBJECT(BSShaderProperty, 0xBA9250);
};

ASSERT_SIZE(BSShaderProperty, 0x60);
ASSERT_SIZE(BSShaderProperty::RenderPass, 0x10);
ASSERT_SIZE(BSShaderProperty::RenderPassArray, 0x14);