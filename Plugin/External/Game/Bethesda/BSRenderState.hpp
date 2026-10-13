#pragma once

#include "Gamebryo/NiStencilProperty.hpp"

enum BSCompareFunctions {
	BSCMP_ALWAYS		= 0,
	BSCMP_LESS			= 1,
	BSCMP_EQUAL			= 2,
	BSCMP_LESSEQUAL		= 3,
	BSCMP_GREATER		= 4,
	BSCMP_NOTEQUAL		= 5,
	BSCMP_GREATEREQUAL	= 6,
	BSCMP_NEVER			= 7,
};

enum BSBlendFunctions {
	BSBLEND_ONE				= 0,
	BSBLEND_ZERO			= 1,
	BSBLEND_SRCCOLOR		= 2,
	BSBLEND_INVSRCCOLOR		= 3,
	BSBLEND_DESTCOLOR		= 4,
	BSBLEND_INVDESTCOLOR	= 5,
	BSBLEND_SRCALPHA		= 6,
	BSBLEND_INVSRCALPHA		= 7,
	BSBLEND_DESTALPHA		= 8,
	BSBLEND_INVDESTALPHA	= 9,
	BSBLEND_SRCALPHASAT		= 10,
};

enum BSStencilBufferOperations {
	BSSTENCILOP_KEEP	= 0,
	BSSTENCILOP_ZERO	= 1,
	BSSTENCILOP_REPLACE = 2,
	BSSTENCILOP_INCR	= 3,
	BSSTENCILOP_DECR	= 4,
	BSSTENCILOP_INVERT	= 5,
};

enum BSRenderStateType : uint32_t {
	BSRS_Z_BUFFER			= 0,
	BSRS_Z_WRITE			= 1,
	BSRS_Z_FUNC				= 2,
	BSRS_DEPTH_BIAS			= 3,
	BSRS_ALPHA_TEST			= 4,
	BSRS_ALPHA_FUNC			= 5,
	BSRS_ALPHA_BLEND		= 6,
	BSRS_ALPHA_BLENDS		= 7,
	BSRS_BLEND_OP			= 8,
	BSRS_STENCIL_ENABLE		= 9,
	BSRS_STENCIL_ACTIONS	= 10,
	BSRS_STENCIL_FUNC		= 11,
	BSRS_STENCIL_WRITE		= 12,
	BSRS_SCISSOR_TEST		= 13,
	// ??					= 14,
	BSRS_CLIP_PLANE			= 15,
	BSRS_COLOR_WRITE		= 16,
	BSRS_ColorWrite123		= 17,
	BSRS_FILL_MODE			= 18,
	BSRS_CULL_MODE			= 19,
	BSRS_ATOC				= 20,
	BSRS_COUNT
};

enum BSRenderStateLock : uint32_t {
	BSRSL_NONE		= 0,
	BSRSL_LOCK		= 1,
};

class BSRenderState {
public:
	static constexpr AddressPtr<uint32_t, 0x11F94B8> eGPUVendor;
	static BSRenderStateType* const uiD3D2BSRS;
	static DWORD* const iLocks;

	static void SetAlphaBlendEnable(bool abEnable, BSRenderStateLock aeLock);
	static void RestoreAlphaBlendEnable(BSRenderStateLock aeLock);

	static void SetColorWriteEnable(uint32_t aMask, BSRenderStateLock aeLock);
	static void RestoreColorWriteEnable(BSRenderStateLock aeLock);

	static void SetZWriteEnable(bool abEnable, BSRenderStateLock aeLock);
	static void RestoreZWriteEnable(BSRenderStateLock aeLock);

	static void SetZEnable(D3DZBUFFERTYPE aeBufferType, BSRenderStateLock aeLock);
	static void RestoreZEnable(BSRenderStateLock aeLock);

	static void SetRenderState(D3DRENDERSTATETYPE aeRenderStateType, DWORD value, BSRenderStateLock aeLock);
};