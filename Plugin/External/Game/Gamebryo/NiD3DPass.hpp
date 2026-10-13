#pragma once

#include "NiD3DPixelShader.hpp"
#include "NiD3DRenderStateGroup.hpp"
#include "NiD3DShaderConstantMap.hpp"
#include "NiD3DTextureStage.hpp"
#include "NiD3DVertexShader.hpp"
#include "NiGeometry.hpp"
#include "NiTArray.hpp"

class NiDX9RenderState;

NiSmartPointer(NiD3DPass);

class NiD3DPass : public NiMemObject {
public:
	NiD3DPass();
	virtual ~NiD3DPass();

	virtual uint32_t	SetupShaderPrograms(NiGeometry* pkGeometry, const NiSkinInstance* pkSkin, const NiSkinPartition::Partition* pkPartition, NiGeometryBufferData* pkBuffData, const NiPropertyState* apProperties, const NiDynamicEffectState* pkEffects, const NiTransform* kWorld, const NiBound* kWorldBound, uint32_t uiPass);
	virtual void		SetupRenderingPass(NiGeometry* pkGeometry, const NiSkinInstance* pkSkin, NiGeometryBufferData* pkBuffData, const NiPropertyState* apProperties, const NiDynamicEffectState* pkEffects, const NiTransform& kWorld, const NiBound& kWorldBound, uint32_t uiPass);
	virtual uint32_t	PostProcessRenderingPass(uint32_t uiPass);

	char											m_szName[16];
	uint32_t										m_uiCurrentStage;
	uint32_t										m_uiStageCount;
	uint32_t										m_uiTexturesPerPass;
	NiTObjectArray<NiPointer<NiD3DTextureStage>>	m_kStages;
	NiD3DRenderStateGroup*							m_pkRenderStateGroup;
	NiD3DShaderConstantMapPtr						m_spPixelConstantMap;
	char*											m_pcPixelShaderProgramFile;
	char*											m_pcPixelShaderEntryPoint;
	char*											m_pcPixelShaderTarget;
	NiD3DPixelShaderPtr								m_spPixelShader;
	NiD3DShaderConstantMapPtr						m_spVertexConstantMap;
	char*											m_pcVertexShaderProgramFile;
	char*											m_pcVertexShaderEntryPoint;
	char*											m_pcVertexShaderTarget;
	float											float58;
	NiD3DVertexShaderPtr							m_spVertexShader;
	bool											m_bSoftwareVP;
	bool											m_bRendererOwned;
	uint32_t										m_uiRefCount;

	NiD3DTextureStage* GetStage(uint32_t auiStage) const {
		return m_kStages.GetAt(auiStage);
	}
};

ASSERT_SIZE(NiD3DPass, 0x68);