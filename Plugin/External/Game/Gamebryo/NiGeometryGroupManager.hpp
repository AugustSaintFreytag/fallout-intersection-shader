#pragma once

#include "NiMemObject.hpp"

class NiGeometryGroup;
class NiScreenTexture;
class NiSkinInstance;
class NiGeometryData;

class NiGeometryGroupManager : public NiMemObject {
public:
	enum RepackingBehavior {
		STATIC = 0x0,
		UNSHARED = 0x1,
		DYNAMIC = 0x2,
	};

	virtual						~NiGeometryGroupManager();
	virtual NiGeometryGroup*	CreateGroup(NiGeometryGroupManager::RepackingBehavior aeBehavior);
	virtual bool				DestroyGroup(NiGeometryGroup* apGroup);
	virtual bool				IsGroupEmpty(NiGeometryGroup* apGroup);
	virtual bool				AddObjectToGroup(NiGeometryGroup* apGroup, NiScreenTexture* apScreenTexture);
	virtual bool				AddObjectToGroupAlt(NiGeometryGroup* apGroup, NiGeometryData* apData, NiSkinInstance* apSkinInstance);
	virtual bool				RemoveObjectFromGroup(NiScreenTexture* apScreenTexture);
	virtual bool				RemoveObjectFromGroupAlt(NiGeometryData* apData, NiSkinInstance* apSkinInstance);
};