#pragma once

#include "NiGPUProgram.hpp"
#include "NiShaderConstantMap.hpp"
#include "NiShaderConstantMapEntry.hpp"
#include "NiSkinPartition.hpp"
#include "NiTArray.hpp"

class NiD3DRenderState;
class NiD3DShaderProgram;
class NiGeometry;
class NiSkinInstance;
class NiGeometryBufferData;
class NiPropertyState;
class NiDynamicEffectState;
class NiSkinPartition;
class NiDX9Renderer;
class NiTransform;
class NiExtraData;

NiSmartPointer(NiD3DShaderConstantMap);

class NiD3DShaderConstantMap : public NiShaderConstantMap {
public:
	NiD3DShaderConstantMap();
	virtual ~NiD3DShaderConstantMap();

	virtual NiRTTI						GetRTTI();
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
	virtual uint32_t					AddPredefinedEntry(const char* apszKey, uint32_t auiExtra, uint32_t auiShaderRegister = 0, const char* apszVariableName = "", uint32_t auiDataSize = 0, uint32_t auiDataStride = 0, void* pvDataSource = 0, bool bCopyData = false);
	virtual uint32_t					RemoveEntry(const char* apszKey);
	virtual NiShaderConstantMapEntry*	GetEntry(const char* apszKey);
	virtual int							GetEntryAtIndex(uint32_t auiIndex);
	virtual uint32_t					SetShaderConstants(NiD3DShaderProgram* apShaderProgram, NiGeometry* apGeometry, const NiSkinInstance* apSkin, const NiSkinPartition::Partition* apPartition, NiGeometryBufferData* apBuffData, const NiPropertyState* apProperties, const NiDynamicEffectState* apEffects, const NiTransform& arWorld, const NiBound& arWorldBound, uint32_t auiPass = 0, bool abGlobal = false);
	virtual uint32_t					GetEntryIndex(const char* apszKey);
	virtual uint32_t					InsertEntry(NiShaderConstantMapEntry* apEntry);
	virtual uint32_t					InsertDefinedEntry(NiShaderConstantMapEntry* apEntry);
	virtual uint32_t					SetupPredefinedEntry(NiShaderConstantMapEntry* apEntry);
	virtual uint32_t					SetDefinedConstant(NiD3DShaderProgram* apShaderProgram, NiShaderConstantMapEntry* apEntry, NiGeometry* apGeometry, const NiSkinInstance* apSkin, const NiSkinPartition::Partition* apPartition, NiGeometryBufferData* apBuffData, const NiPropertyState* apProperties, const NiDynamicEffectState* apEffects, const NiTransform& arWorld, const NiBound& arWorldBound, uint32_t auiPass);
	virtual uint32_t					SetConstantConstant(NiD3DShaderProgram* apShaderProgram, NiShaderConstantMapEntry* apEntry, uint32_t auiPass);
	virtual uint32_t					SetAttributeConstant(NiD3DShaderProgram* apShaderProgram, NiShaderConstantMapEntry* apEntry, NiGeometry* apGeometry, const NiSkinInstance* apSkin, const NiSkinPartition::Partition* apPartition, NiGeometryBufferData* apBuffData, const NiPropertyState* apProperties, const NiDynamicEffectState* apEffects, const NiTransform& arWorld, const NiBound& arWorldBound, uint32_t auiPass, bool abGlobal, NiExtraData* apExtraData);
	virtual uint32_t					SetGlobalConstant(NiD3DShaderProgram* apShaderProgram, NiShaderConstantMapEntry* apEntry, uint32_t auiPass);
	virtual uint32_t					SetOperatorConstant(NiD3DShaderProgram* apShaderProgram, NiShaderConstantMapEntry* apEntry, NiGeometry* apGeometry, const NiSkinInstance* apSkin, const NiPropertyState* apProperties, const NiDynamicEffectState* apEffects, const NiTransform& arWorld, const NiBound& arWorldBound, uint32_t auiPass);
	virtual uint32_t					SetupObjectEntry(NiShaderConstantMapEntry* apEntry);
	virtual uint32_t					SetObjectConstant(NiD3DShaderProgram* apShaderProgram, NiShaderConstantMapEntry* apEntry, NiGeometry* apGeometry, const NiSkinInstance* apSkin, const NiSkinPartition::Partition* apPartition, NiGeometryBufferData* apBuffData, const NiPropertyState* apProperties, const NiDynamicEffectState* apEffects, const NiTransform& arWorld, const NiBound& arWorldBound, uint32_t auiPass);

	NiTObjectArray<NiShaderConstantMapEntryPtr>	m_aspEntries;
	NiTObjectArray<NiShaderConstantMapEntryPtr>	m_aspDefinedEntries;
	bool										m_bModified;
	NiD3DShaderProgram*							m_pkLastShaderProgram;
	uint32_t									m_eLastError;
	LPDIRECT3DDEVICE9							m_pkD3DDevice;
	NiDX9Renderer*								m_pkD3DRenderer;
	NiD3DRenderState*							m_pkD3DRenderState;

	NIRTTI_ADDRESS(0x126F7C4);
};

ASSERT_SIZE(NiD3DShaderConstantMap, 0x44)