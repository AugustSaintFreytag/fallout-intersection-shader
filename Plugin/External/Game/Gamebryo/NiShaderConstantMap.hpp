#pragma once

#include "NiRefObject.hpp"
#include "NiGPUProgram.hpp"
#include "NiShaderAttributeDesc.hpp"

class NiShaderConstantMap : public NiRefObject {
public:
	NiShaderConstantMap();
	virtual ~NiShaderConstantMap();

	virtual uint32_t	AddEntry(const char* pszKey, uint32_t uiFlags, uint32_t uiExtra, uint32_t uiShaderRegister, uint32_t uiRegisterCount, const char* pszVariableName = "", uint32_t uiDataSize = 0, uint32_t uiDataStride = 0, const void* pvDataSource = 0, bool bCopyData = false);
	virtual uint32_t	AddEntryAlt(const char* pszKey, uint32_t uiExtra, uint32_t uiShaderRegister, const char* pszVariableName);
	virtual uint32_t	AddAttributeEntry(const char* pszKey, uint32_t uiFlags, uint32_t uiExtra, uint32_t uiShaderRegister, uint32_t uiRegisterCount, const char* pszVariableName, uint32_t uiDataSize, uint32_t uiDataStride, const void* pvDataSource, bool bCopyData = false);
	virtual uint32_t	AddConstantEntry(const char* pszKey, uint32_t uiFlags, uint32_t uiExtra, uint32_t uiShaderRegister, uint32_t uiRegisterCount, const char* pszVariableName, uint32_t uiDataSize, uint32_t uiDataStride, const void* pvDataSource, bool bCopyData = false);
	virtual uint32_t	AddGlobalEntry(const char* pszKey, uint32_t uiFlags, uint32_t uiExtra, uint32_t uiShaderRegister, uint32_t uiRegisterCount, const char* pszVariableName, uint32_t uiDataSize, uint32_t uiDataStride, const void* pvDataSource, bool bCopyData = false);
	virtual uint32_t	AddOperatorEntry(const char* pszKey, uint32_t uiFlags, uint32_t uiExtra, uint32_t uiShaderRegister, uint32_t uiRegisterCount, const char* pszVariableName);
	virtual uint32_t	AddObjectEntry(const char* pszKey, uint32_t uiShaderRegister, const char* pszVariableName, uint32_t uiObjectIndex, NiShaderAttributeDesc::ObjectType eObjectType);
	virtual void		SortByEnabled();

	NiGPUProgram::ProgramType m_eProgramType;
};

ASSERT_SIZE(NiShaderConstantMap, 0xC);