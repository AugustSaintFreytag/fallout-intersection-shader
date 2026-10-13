#pragma once

#include "BSShaderManager.hpp"
#include "Gamebryo/NiObject.hpp"
#include "Gamebryo/NiTMap.hpp"
#include "Gamebryo/NiTPointerList.hpp"

class BSShader;
class NiTexture;

// BSSimpleArray<BSBatchRenderer::PassGroup, 1024> - PS3, FO3

class BSBatchRenderer : public NiObject {
public:
	virtual void RegisterPass(BSShaderProperty::RenderPass* apRenderPass, uint32_t auiPassEnum);
	virtual void RenderBatches(BSShaderManager::RenderPassType aeMinPass, BSShaderManager::RenderPassType aeMaxPass, bool abBlendAlpha);

	enum GroupType : int32_t {
		GROUP_NONE							= -1,
		GROUP_LOD							= 0,
		GROUP_UNK_1							= 1,
		GROUP_BLENDED_DECAL_SINGLE			= 2,
		GROUP_BLENDED_DECAL_MULTI			= 3,
		GROUP_BLENDED_DECAL_SINGLE_DEBUG	= 4,
		GROUP_BLENDED_DECAL_MULTI_DEBUG		= 5,
		GROUP_REFRACT						= 6,
		GROUP_UNK_7							= 7,
		GROUP_REFRACT_CLEAR					= 8,
		GROUP_UNK_9							= 9,
		GROUP_UNK_10						= 10,
		GROUP_DEPTH_WORLD					= 11,
		GROUP_DEPTH_PLAYER					= 12,
		GROUP_COUNT
	};

	enum AlphaGroupType : uint32_t {
		ALPHA_GROUP_SURFACE		= 0,
		ALPHA_GROUP_UNDERWATER	= 1,
		ALPHA_GROUP_COUNT
	};

	enum TransparencyType : uint32_t {
		ALPHA_NONE				= 0,
		ALPHA_STENCIL			= 1,
		ALPHA_TEST_STENCIL		= 2,
		ALPHA_TEST_NO_STENCIL	= 3,
		ALPHA_NO_TMSAA			= 4,
		ALPHA_COUNT
	};

	struct GeometryGroup {
		GeometryGroup(BSShaderManager::RenderPassType aeMaxPass = BSShaderManager::BSSM_FINAL_PASS);
		~GeometryGroup();

		enum Flags {
			NO_SORT = 1 << 0,
		};

		Bitfield8							ucFlags;
		uint16_t							usCount;
		BSShaderManager::RenderPassType		eMaxPass;
		BSBatchRenderer*					pBatchRenderer;
		uint32_t							pad[5];
		float								fDepth;
	};

	struct PassGroup {
		uint32_t			pad[25];
		uint32_t			uiCount;
	};

	PassGroup*											pPassGroups;
	NiTMap<BSShaderManager::RenderPassType, uint32_t>	kPassMap; // PassType -> PassGroup index
	uint32_t											uiMaxPass;
	BSShaderManager::RenderPassType						ePassStart;
	BSShaderManager::RenderPassType						ePassEnd;
	DWORD*												pTextureBlendStagesList;
	uint32_t											pad0[2];
	int32_t												iGroupingAlphasGroupCount;
	bool												bAutoClearPasses;
	GeometryGroup*										pGeometryGroups[GROUP_COUNT];
	GeometryGroup*										pUngroupedAlphas[ALPHA_GROUP_COUNT];
	NiTPointerList<GeometryGroup*>						kGroupingAlphasGroup[ALPHA_GROUP_COUNT];
	GeometryGroup*										pGroupedAlphas[ALPHA_GROUP_COUNT];
};

ASSERT_SIZE(BSBatchRenderer, 0x98);