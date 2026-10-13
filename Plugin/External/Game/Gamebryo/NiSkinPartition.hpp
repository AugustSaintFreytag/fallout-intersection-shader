#pragma once

#include "NiObject.hpp"
#include "NiTSet.hpp"

NiSmartPointer(NiSkinPartition);

class NiGeometryBufferData;
class NiTriBasedGeomData;
class NiSkinData;

class NiSkinPartition : public NiObject {
public:
	NiSkinPartition();
	virtual ~NiSkinPartition();

	class Interaction : public NiMemObject {
	public:
		uint32_t	m_uiBone;
		float		m_fWeight;
	};

	class Partition : public NiMemObject {
	public:
		virtual ~Partition();

		uint16_t*				m_pusBones;
		float*					m_pfWeights;
		uint16_t*				m_pusVertexMap;
		uint8_t*				m_pucBonePalette;
		uint16_t*				m_pusTriList;
		uint16_t*				m_pusStripLengths;
		uint16_t				m_usVertices;
		uint16_t				m_usTriangles;
		uint16_t				m_usBones;
		uint16_t				m_usStrips;
		uint16_t				m_usBonesPerVertex;
		NiGeometryBufferData*	m_pkBuffData;
	};

	class VertexInteractions : public NiTPrimitiveSet<Interaction> {
	public:
	};

	class BoneSet : public NiUnsignedShortSet {
	public:
	};

	class TriangleSet : public NiUnsignedShortSet {
	public:
	};

	class BoneSetMerger : public NiMemObject {
	public:
		uint32_t		m_uiIndex[2];
		uint16_t		m_usEl[2];
		const BoneSet&	m_kSet0;
		const BoneSet&	m_kSet1;
	};

	typedef NiTPrimitiveSet<BoneSet*> NiBoneSetCollection;

	uint32_t	m_uiPartitions;
	Partition*	m_pkPartitions;

	CREATE_OBJECT(NiSkinPartition, 0xA81FB0);
	NIRTTI_ADDRESS(0x11F5B1C);
};

ASSERT_SIZE(NiSkinPartition, 0x10);