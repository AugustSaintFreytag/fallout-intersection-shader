#pragma once

#include "Gamebryo/NiTPointerList.hpp"
#include "Gamebryo/NiSmartPointer.hpp"

class NiDX9Renderer;
class NiTexture;
class NiDepthStencilBuffer;
class NiObject;
class NiCamera;
class BSRenderedTexture;

/*
Stores and manages rendered textures. 
*/
class BSTextureManager {
public:
	enum eTextureType {
		BSTM_RT_CUSTOM								= -1,
		BSTM_RT_HDR_DOWNSAMPLE						= 0,
		BSTM_RT_HDR_DOWNSAMPLE_ANISOINTERMEDIATE	= 1,
		BSTM_RT_HDR_BLURSWAP						= 2,
		BSTM_RT_IMAGESPACE_SWAP						= 3,
		BSTM_RT_MAIN_IMAGESPACE						= 4,
		BSTM_RT_MAIN_FIRSTPERSON					= 5,
		BSTM_RT_MAIN_MENUBG							= 6,
		BSTM_RT_TESWATER_FFT						= 7,
		BSTM_RT_TESWATER_GREY						= 8,
		BSTM_RT_TESWATER_REFLECTION					= 9,
		BSTM_RT_TESWATER_LOD_REFLECTION				= 10,
		BSTM_RT_TESWATER_REFRACTION					= 11,
		BSTM_RT_TESWATER_RAIN						= 12,
		BSTM_RT_TESWATER_SCRAMBLEMAP				= 13,
		BSTM_RT_TESWATER_IFFT_BUTTERFLYMAP			= 14,
		BSTM_RT_TESWATER_HIGH_W						= 15,
		BSTM_RT_TESWATER_HIGH_AMPLITUDE				= 16,
		BSTM_RT_TESWATER_DEPTH						= 17,
		BSTM_RT_TESWATER_NOISE_HEIGHTMAP			= 18,
		BSTM_RT_TESWATER_NOISE_NORMALMAP			= 19,
		BSTM_RT_TESWATER_DISPLACEMENT				= 20,
		BSTM_RT_TESWATER_DEBUG_DISPLAY				= 21,
		BSTM_RT_TESWATER_BLUR						= 22,
		BSTM_RT_TREES_CANOPYSHADOWMASK				= 23,
		BSTM_RT_BLURFULL_BUFFER						= 24,
		BSTM_RT_BLURFULL_SWAP						= 25,
		BSTM_RT_BLUR20_BUFFER						= 26,
		BSTM_RT_GETHIT_BUFFER						= 27,
		BSTM_RT_LOCALMAP_FINAL						= 28,
		BSTM_RT_LOCALMAP_COLOR						= 29,
		BSTM_RT_LOCALMAP_NORMALS					= 30,
		BSTM_RT_REFRACTION_NORMALS					= 31,
		BSTM_RT_VFOG								= 32,
		BSTM_RT_SCENE_DEPTH_BUFFER					= 33,
		BSTM_RT_VFOG_SWAP							= 34,
		BSTM_RT_VELOCITY							= 35,
		BSTM_RT_VELOCITY_SWAP						= 36,
		BSTM_RT_DOF_BLUR							= 37,
		BSTM_RT_DOF_BLUR_SWAP						= 38,
		BSTM_RT_DOF_MASK							= 39,
		BSTM_RT_DEPTH_RADIALBLUR_BLUR				= 40,
		BSTM_RT_CUBEMAP_CAMERA						= 41,
		BSTM_RT_SHADOWS_SHADOWMAP					= 42,
		BSTM_RT_SHADOWS_SHADOWMAPFRUSTUM			= 43,
		BSTM_RT_ENVMAP_WINDOWS						= 44,
		BSTM_RT_INTERFACE_RENDEREDMENU				= 45,
		BSTM_RT_INTERFACE_RENDEREDMENU_SWAP			= 46,
		BSTM_RT_INTERFACE_VATS						= 47,
		BSTM_RT_INTERFACE_TEMP						= 48,
		BSTM_RT_NOISE_BUFFER						= 49,
		BSTM_RT_UNK_50								= 50,
		BSTM_RT_DECAL_OCCLUSION						= 51,
		BSTM_RT_MAX,
	};

	enum CreationFlags {
		BSTM_CF_NONE				= 0,
		BSTM_CF_UNK_1				= 1 << 0,
		BSTM_CF_UNK_2				= 1 << 1,
		BSTM_CF_NO_STENCIL			= 1 << 2,
		BSTM_CF_NO_RENDER_TARGET	= 1 << 3,
		BSTM_CF_CUBE_MAP			= 1 << 4,
		BSTM_CF_UNK_20				= 1 << 5,
		BSTM_CF_USE_MSAA			= 1 << 6,
		BSTM_CF_NO_DEPTH_DISCARD	= 1 << 7,
		BSTM_CF_NO_DEPTH			= 1 << 8,
		BSTM_CF_DONT_FREE			= 1 << 9,
		BSTM_CF_UNK_400				= 1 << 10,
	};

	class RenderedTextureEntry {
	public:
		NiPointer<BSRenderedTexture>	spTexture;
		D3DFORMAT						eFormat;
		RenderedTextureEntry*			pNext;
		Bitfield32						uiFlags;
		bool							bLoaded;
		eTextureType					eType;
	};

	struct TextureLists {
		NiTPointerList<NiPointer<BSRenderedTexture>> kShadowMaps;
		NiTPointerList<NiPointer<BSRenderedTexture>> kWaterDepthMaps;
		NiTPointerList<NiPointer<BSRenderedTexture>> kWaterReflectionMaps;
		NiTPointerList<NiPointer<BSRenderedTexture>> kWaterBlurMaps;
		NiTPointerList<NiPointer<BSRenderedTexture>> kWaterNoiseMaps;
	};
	

	TextureLists							kUnusedShaderTextures;
	TextureLists							kUsedShaderTextures;
	NiTPointerList<RenderedTextureEntry*>	kUnusedTextures;
	NiTPointerList<RenderedTextureEntry*>	kUsedTextures;
	NiTPointerList<BSRenderedTexture*>		kUnusedShadowMaps;
	NiTPointerList<BSRenderedTexture*>		kUsedShadowMaps;
	NiPointer<NiDepthStencilBuffer>			spShadowDepthBuffer;
	NiPointer<NiObject>						spUnk0AC;

	static constexpr AddressPtr<uint32_t, 0x11A9608> uiMaxAnisotropy;

	BSRenderedTexture* BorrowRenderedTexture(NiDX9Renderer* apRenderer, eTextureType aeType, uint32_t auiFlags = 0, uint32_t unk = 0, uint32_t auiBackgroundColor = 0);
	void ReturnRenderedTexture(BSRenderedTexture* apRenderedTexture);
};

ASSERT_SIZE(BSTextureManager, 0xB0);