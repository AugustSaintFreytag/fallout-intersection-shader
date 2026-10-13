#pragma once

#include "NiAVObject.hpp"
#include "NiSkinPartition.hpp"
#include "NiSkinData.hpp"

NiSmartPointer(NiSkinInstance);

class NiSkinInstance : public NiObject {
public:
	NiSkinInstance();
	virtual ~NiSkinInstance();

	NiSkinDataPtr		m_spSkinData;
	NiSkinPartitionPtr	m_spSkinPartition;
	NiAVObject*			m_pkRootParent;
	NiAVObject**		m_ppkBones;
	uint32_t			m_uiFrameID;
	uint32_t			m_uiNumMatrices;
	uint32_t			m_uiNumRegisters;
	uint32_t			m_uiAllocatedSize;
	void*				m_pvBoneMatrices;
	void*				unk2C;
	void*				m_pvSkinToWorldWorldToSkinMatrix;

	CREATE_OBJECT(NiSkinInstance, 0xA866F0);
	NIRTTI_ADDRESS(0x11F5B2C);
};

ASSERT_SIZE(NiSkinInstance, 0x34);