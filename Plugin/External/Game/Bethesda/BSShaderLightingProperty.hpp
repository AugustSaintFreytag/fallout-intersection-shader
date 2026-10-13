#pragma once

#include "BSShaderProperty.hpp"
#include "Gamebryo/NiTPointerList.hpp"

NiSmartPointer(BSShaderLightingProperty);

class BSShaderLightingProperty : public BSShaderProperty {
public:
	BSShaderLightingProperty();
	virtual ~BSShaderLightingProperty();

	virtual void CopyTo2(BSShaderLightingProperty* apShaderProp);
	virtual void Unk_31(BSShaderLightingProperty* apShaderProp);

	NiTPointerList<ShadowSceneLight*>	kLightList;
	float								fForcedDarkness;
	uint32_t							uiRefID;
	bool								bLightListChanged;
	NiTListIterator						kLightIterator;

	CREATE_OBJECT(BSShaderLightingProperty, 0xB71920);
	NIRTTI_ADDRESS(0x11FA560);

	static constexpr AddressPtr<ShadowSceneLight, 0x11FA310> kEmptyLight;
};

ASSERT_SIZE(BSShaderLightingProperty, 0x7C);