#pragma once

#include "NiShaderDeclaration.hpp"
#include "NiSkinInstance.hpp"
#include "NiSkinPartition.hpp"

class NiDX9VertexBufferManager;
class NiVBChip;
class NiGeometryData;

NiSmartPointer(NiD3DShaderDeclaration);

class NiD3DShaderDeclaration : public NiShaderDeclaration {
public:
	enum ShaderParameterTesselator {
		SPTESS_DEFAULT,
		SPTESS_PARTIALU,
		SPTESS_PARTIALV,
		SPTESS_CROSSUV,
		SPTESS_UV,
		SPTESS_LOOKUP,
		SPTESS_LOOKUPPRESAMPLED,
		SPTESS_COUNT
	};

	NiD3DShaderDeclaration();
	virtual ~NiD3DShaderDeclaration();

	virtual bool							SetEntryAlt(uint32_t uiStream, uint32_t uiEntry, uint32_t uiRegister, NiShaderDeclaration::ShaderParameter eInput, NiShaderDeclaration::ShaderParameterType eType, NiShaderDeclaration::ShaderParameterUsage eUsage, uint32_t uiIndex, NiD3DShaderDeclaration::ShaderParameterTesselator eTess);
	virtual void							SetConstantData(uint32_t uiStartRegister, uint32_t uiNumRegisters, float* pfData);
	virtual void							ClearAllEntries(uint32_t uiStream);
	virtual uint32_t						GetVertexStride(uint32_t uiStream);
	virtual void							SetSoftwareVertexProcessing();
	virtual bool							GetSoftwareVertexProcessing();
	virtual NiVBChip*						PackUntransformedVB(NiGeometryData* pkData, NiSkinInstance* pkSkin, NiSkinPartition::Partition* pkPartition, uint16_t usDirtyFlags, NiVBChip* pkOldVBChip, uint32_t uiStream, void* pvLockedBuff = nullptr, void* a = nullptr, void* b = nullptr);
	virtual bool							GenerateVertexParameters(LPDIRECT3DVERTEXDECLARATION9* pkDeclaration, uint32_t* uiNumStreams);
	virtual LPDIRECT3DVERTEXDECLARATION9	GetD3DDeclaration();

	class ShaderRegisterEntry : public NiMemObject {
	public:
		uint32_t					m_uiRegister;
		uint32_t					m_uiPackingOffset;
		ShaderParameter				m_eInput;
		ShaderParameterType			m_eType;
		ShaderParameterTesselator	m_eTesselator;
		ShaderParameterUsage		m_eUsage;
		uint32_t					m_uiUsageIndex;
	};


	class ShaderRegisterStream : public NiMemObject {
	public:
		bool					m_bValid;
		uint32_t				m_uiEntryCount;
		ShaderRegisterEntry*	m_pkEntries;
		uint32_t				m_uiStride;
	};

	NiDX9Renderer*				m_pkRenderer;
	NiDX9VertexBufferManager*	m_pkVBManager;
	LPDIRECT3DDEVICE9			m_pkD3DDevice;
	uint32_t					m_uiDeclSize;
	uint32_t					m_uiCurrDeclIndex;
	uint32_t					m_uiMaxStreamEntryCount;
	uint32_t					m_uiStreamCount;
	ShaderRegisterStream*		m_pkStreamEntries;
	bool						m_bModified;
};

ASSERT_SIZE(NiD3DShaderDeclaration, 0x02C);