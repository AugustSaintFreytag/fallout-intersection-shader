#pragma once

#include "NiD3DShader.hpp"
#include "NiDynamicEffectState.hpp"

class NiD3DDefaultShader : public NiD3DShader {
public:
	enum PassFogStatus {
		NO_FOG			= 0,
		BLACK_FOG		= 1,
		STANDARD_FOG	= 2,
	};

	bool							Unk3C;
	bool							Unk3D;
	uint32_t						m_uiMaxTextureIndex;
	uint32_t						m_uiRemainingStages;
	uint32_t						m_uiRemainingTextures;
	uint32_t						m_uiOldLightingValue;
	PassFogStatus*					m_peFogPassArray;
	uint32_t						m_uiFogPassArraySize;
	const NiDynamicEffectState*		m_pkLastState;

	NIRTTI_ADDRESS(0x126F554);
};

ASSERT_SIZE(NiD3DDefaultShader, 0x5C);