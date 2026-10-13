#pragma once

#include "NiRefObject.hpp"
#include "NiFixedString.hpp"

NiSmartPointer(NiShaderConstantMapEntry);

class NiShaderConstantMapEntry : public NiRefObject {
public:
	bool			bEnabled;
	NiFixedString	m_kKey;
	uint32_t		m_uiInternal;
	Bitfield32		m_uiFlags;
	uint32_t		m_uiExtra;
	uint32_t		m_uiShaderRegister;
	uint32_t		m_uiRegisterCount;
	NiFixedString	m_kVariableName;
	uint32_t		m_uiDataSize;
	uint32_t		m_uiDataStride;
	void*			m_pvDataSource;
	bool			m_bOwnData;
	bool			m_bVariableHookupValid;
	bool			m_bColumnMajor;
};

ASSERT_SIZE(NiShaderConstantMapEntry, 0x38);