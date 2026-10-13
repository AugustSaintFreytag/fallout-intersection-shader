#pragma once

#include "NiMemObject.hpp"

class NiDX9RenderState;

class NiD3DTextureStageGroup : public NiMemObject {
public:
	bool		m_bRendererOwned;
	uint32_t	m_uiNumSavedStages;
	uint32_t	m_uiSavedStageArrayIter;
	uint32_t	m_auiSavedStageArray[8];
	bool		m_abSavedStageValid[8];
	uint32_t	m_uiNumStages;
	uint32_t	m_uiStageArrayIter;
	uint32_t	m_auiStageArray[8];
	bool		m_abStageValid[8];
	uint32_t	m_uiNumSavedSamplers;
	uint32_t	m_uiSavedSamplerArrayIter;
	uint32_t	m_auiSavedSamplerArray[5];
	bool		m_abSavedSamplerValid[5];
	uint32_t	m_uiNumSamplers;
	uint32_t	m_uiSamplerArrayIter;
	uint32_t	m_auiSamplerArray[5];
	bool		m_abSamplerValid[5];
	uint32_t	m_uiNumUseMapValue;
	bool		m_abSamplerUseMapValue[5];
};

ASSERT_SIZE(NiD3DTextureStageGroup, 0xB8);