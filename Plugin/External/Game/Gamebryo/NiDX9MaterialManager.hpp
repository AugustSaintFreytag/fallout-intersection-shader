#pragma once

#include <d3d9types.h>

class NiDX9MaterialManager : public NiMemObject {
public:
	struct MaterialEntry {
		D3DMATERIAL9	m_kD3DMat;
		uint32_t		m_uiRevID;
	};

	MaterialEntry*	m_pkCurrentMaterial;
	bool			m_bChanged;
};

ASSERT_SIZE(NiDX9MaterialManager, 0x8);