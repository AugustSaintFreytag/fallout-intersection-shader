#pragma once

#include "NiProperty.hpp"
#include "NiTexture.hpp"

class NiTextureTransform;

NiSmartPointer(NiTexturingProperty);

class NiTexturingProperty : public NiProperty {
public:
	NiTexturingProperty();
	virtual ~NiTexturingProperty();

	enum ApplyMode {
		APPLY_REPLACE		= 0,
		APPLY_DECAL			= 1,
		APPLY_MODULATE		= 2,
		APPLY_DEPRECATED	= 3,
		APPLY_DEPRECATED2	= 4,
		APPLY_MAX_MODES,
	};

	enum FilterMode {
		FILTER_NEAREST				= 0,
		FILTER_BILERP				= 1,
		FILTER_TRILERP				= 2,
		FILTER_NEAREST_MIPNEAREST	= 3,
		FILTER_NEAREST_MIPLERP		= 4,
		FILTER_BILERP_MIPNEAREST	= 5,
		FILTER_ANISOTROPY			= 6,
		FILTER_ANISOTROPY_2			= 7,
		FILTER_MAX_MODES,
	};

	enum ClampMode {
		CLAMP_S_CLAMP_T = 0,
		CLAMP_S_WRAP_T	= 1,
		WRAP_S_CLAMP_T	= 2,
		WRAP_S_WRAP_T	= 3,
		CLAMP_MAX_MODES,
	};

	enum Flags : uint16_t {
		MULTITEXTURE_MASK	= 0x1,
		APPLYMODE_MASK		= 0xE,
		APPLYMODE_POS		= 0x1,
		DECALCOUNT_MASK		= 0xFF0,
		DECALCOUNT_POS		= 0x4,
	};

	class Map : public NiMemObject {
	public:
		Map();
		virtual				~Map();
		virtual void		LoadBinary(NiStream& arStream);
		virtual void		SaveBinary(NiStream& arStream);
		virtual uint32_t	GetClassID() const;

		Bitfield16			m_usFlags;
		NiTexture*			m_spTexture;
		NiTextureTransform* m_pkTextureTransform;
	};

	class ShaderMap : public Map {
	public:
		uint32_t m_uiID;
	};

	Bitfield16										m_usFlags;
	NiTPrimitiveArray<NiTexturingProperty::Map*>	m_kMaps;
	NiTPrimitiveArray<ShaderMap>*					m_pkShaderMaps;

	CREATE_OBJECT(NiTexturingProperty, 0xA6AD40);
	NIRTTI_ADDRESS(0x11F49A4);
};

ASSERT_SIZE(NiTexturingProperty, 0x30)