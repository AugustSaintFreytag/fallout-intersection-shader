#pragma once

#include "NiRefObject.hpp"
#include "NiFixedString.hpp"
#include "NiRTTI.hpp"
#include "NiGeometry.hpp"

class NiDynamicEffectState;
class NiGeometryBufferData;
class NiShaderConstantMap;
class NiGPUProgram;
class NiSkinInstance;

class NiShader : public NiRefObject {
public:
	NiShader();
	virtual ~NiShader();

	virtual NiRTTI*						GetRTTI();
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
	virtual bool						IsInitialized();
	virtual void						Initialize();
	virtual bool						SetupGeometry(NiGeometry* apGeometry);
	virtual uint32_t					PreProcessPipeline(NiGeometry* apGeometry, const NiSkinInstance* apSkin, NiGeometryBufferData* apRendererData, const NiPropertyState* apProperties, const NiDynamicEffectState* apEffects, const NiTransform& arWorld, const NiBound& arWorldBound);
	virtual void						UpdatePipeline(NiGeometry* apGeometry, const NiSkinInstance* apSkin, NiGeometryBufferData* apRendererData, const NiPropertyState* apProperties, const NiDynamicEffectState* apEffects, const NiTransform& arWorld, const NiBound& arWorldBound);
	virtual void						SetupRenderingPass(NiGeometry* apGeometry, const NiSkinInstance* apSkin, NiGeometryBufferData* apRendererData, const NiPropertyState* apProperties, const NiDynamicEffectState* apEffects, const NiTransform& arWorld, const NiBound& arWorldBound);
	virtual uint32_t					SetupTransformations(NiGeometry* apGeometry, const NiSkinInstance* apSkin, const NiSkinPartition::Partition* apPartition, NiGeometryBufferData* apRendererData, const NiPropertyState* apProperties, const NiDynamicEffectState* apEffects, const NiTransform& arWorld, const NiBound& arWorldBound);
	virtual uint32_t					SetupTransformationsAlt(NiGeometry* apGeometry, const NiSkinInstance* apSkin, const NiSkinPartition::Partition* apPartition, NiGeometryBufferData* apRendererData, const NiPropertyState* apProperties, const NiDynamicEffectState* apEffects, const NiTransform& arWorld, const NiBound& arWorldBound);
	virtual uint32_t					SetupShaderPrograms(NiGeometry* apGeometry, const NiSkinInstance* apSkin, const NiSkinPartition::Partition* apPartition, NiGeometryBufferData* apRendererData, const NiPropertyState* apProperties, const NiDynamicEffectState* apEffects, const NiTransform& arWorld, const NiBound& arWorldBound);
	virtual NiGeometryBufferData*		PrepareGeometryForRendering(NiGeometry* apGeometry, const NiSkinPartition::Partition* apPartition, NiGeometryBufferData* apRendererData, const NiPropertyState* apProperties);
	virtual uint32_t					PostRender(NiGeometry* apGeometry, const NiSkinInstance* apSkin, const NiSkinPartition::Partition* apPartition, NiGeometryBufferData* apRendererData, const NiPropertyState* apProperties, const NiDynamicEffectState* apEffects, const NiTransform& arWorld, const NiBound& arWorldBound);
	virtual uint32_t					PostProcessPipeline(NiGeometry* apGeometry, const NiSkinInstance* apSkin, NiGeometryBufferData* apRendererData, const NiPropertyState* apProperties, const NiDynamicEffectState* apEffects, const NiTransform& arWorld, const NiBound& arWorldBound);
	virtual void						SetupTextures(const NiPropertyState* apProperties);
	virtual void						UpdateConstants(const NiPropertyState* apProperties);
	virtual void						SetBlendAlpha(const NiPropertyState* apProperties);
	virtual void						SetTestAlpha(const NiPropertyState* apProperties);
	virtual void						PreRenderSetup(const NiPropertyState* apProperties, bool abFirstPass);
	virtual void						PostRenderSetup(const NiPropertyState* apProperties);
	virtual int							HasPasses();
	virtual int							EndPass();
	virtual void						StreamCanSkip();
	virtual NiShaderDeclaration*		CreateShaderDeclaration(uint32_t auiMaxStreamEntryCount, uint32_t auiStreamCount = 1);
	virtual bool						AppendRenderPass(uint32_t& auiPassId);
	virtual bool						SetAlphaOverride(uint32_t auiPassId, bool abAlphaBlend, bool abUsePreviousSrcBlendMode, NiAlphaProperty::AlphaFunction aeSrcBlendMode, bool abUsePreviousDestBlendMode, NiAlphaProperty::AlphaFunction aeDestBlendMode);
	virtual void						SetUsesNiRenderState(bool abUses);
	virtual bool						SetGPUProgram(uint32_t auiPassId, NiGPUProgram* apProgram, uint32_t& auiProgramId);
	virtual NiShaderConstantMap*		CreateShaderConstantMap(uint32_t auiPassId, uint32_t auiProgramId);
	virtual bool						AppendTextureSampler(uint32_t auiPassId, uint32_t& auiSamplerId, const NiFixedString& arSemantic, const NiFixedString& arVariableName, uint32_t auiInstance = 0);
	virtual void*						GetShaderInstanceDesc();

	NiFixedString	m_kName;
	uint32_t		m_uiImplementation;
	bool			m_bInitialized;

	NIRTTI_ADDRESS(0x11F5D08);
};

ASSERT_SIZE(NiShader, 0x14);