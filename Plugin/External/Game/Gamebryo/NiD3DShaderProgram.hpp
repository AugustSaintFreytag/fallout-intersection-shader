#pragma once

#include "NiGPUProgram.hpp"
#include "NiObject.hpp"

class NiD3DShaderProgramCreator;
class NiShaderConstantMapEntry;
class NiD3DRenderState;
class NiDX9Renderer;

class NiD3DShaderProgram : public NiGPUProgram {
public:
	NiD3DShaderProgram();
	virtual ~NiD3DShaderProgram();

	virtual const NiRTTI*				GetRTTI() const;
	virtual NiNode*						IsNode();
	virtual BSFadeNode*					IsFadeNode();
	virtual BSMultiBoundNode*			IsMultiBoundNode();
	virtual NiGeometry*					IsGeometry();
	virtual NiTriBasedGeom*				IsTriBasedGeometry();
	virtual NiTriStrips*				IsTriStrips();
	virtual NiTriShape*					IsTriShape();
	virtual BSSegmentedTriShape*		IsSegmentedTriShape();
	virtual BSResizableTriShape*		IsResizableTriShape();
	virtual NiParticles*				IsParticlesGeom();
	virtual NiLines*					IsLinesGeom();
	virtual bhkNiCollisionObject*		IsBhkNiCollisionObject();
	virtual bhkBlendCollisionObject*	IsBhkBlendCollisionObject();
	virtual bhkRigidBody*				IsBhkRigidBody();
	virtual bhkLimitedHingeConstraint*	IsBhkLimitedHingeConstraint();
	virtual uint32_t					GetVariableCount() const;
	virtual const char*					GetVariableName() const;
	virtual const char*					GetName() const;
	virtual const char*					SetName(const char* pszName);
	virtual char*						GetShaderProgramName();
	virtual void						SetShaderProgramName(const char* pszName);
	virtual uint32_t					GetCodeSize() const;
	virtual void*						GetCode() const;
	virtual void						SetCode(uint32_t uiSize, void* pvCode);
	virtual NiD3DShaderProgramCreator*	GetCreator() const;
	virtual void						SetCreator(NiD3DShaderProgramCreator* pkCreator);
	virtual void						SetShaderConstant(NiShaderConstantMapEntry*, const void*, uint32_t);
	virtual bool						SetShaderConstantArray(NiShaderConstantMapEntry* pkEntry, const void* pvDataSource, uint32_t uiNumEntries, uint32_t uiRegistersPerEntry, uint16_t* pusReorderArray);

	char*						m_pszName;
	union {
		struct {
			char*						m_pszShaderProgramName;
			uint32_t					m_uiCodeSize;
			void*						m_pvCode;
			NiD3DShaderProgramCreator*	m_pkCreator;
		} kGBData;

		struct {
			void*		pShaderProg[3];
			IUnknown*	pShaderHandleBackup;
		} kNVRData;
	};
	LPDIRECT3DDEVICE9			m_pkD3DDevice;
	NiDX9Renderer*				m_pkD3DRenderer;
	NiD3DRenderState*			m_pkD3DRenderState;

	NIRTTI_ADDRESS(0x126F9A4);
};

ASSERT_SIZE(NiD3DShaderProgram, 0x2C);