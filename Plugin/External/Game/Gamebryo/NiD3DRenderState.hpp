#pragma once

#include "NiRefObject.hpp"
#include "NiColor.hpp"
#include "NiDX9MaterialManager.hpp"
#include "NiStencilProperty.hpp"
#include "NiAlphaProperty.hpp"
#include "NiTransform.hpp"

class NiDitherProperty;
class NiDX9Renderer;
class NiDX9ShaderConstantManager;
class NiFogProperty;
class NiFogProperty;
class NiMaterialProperty;
class NiPropertyState;
class NiShadeProperty;
class NiSpecularProperty;
class NiSpecularProperty;
class NiStencilProperty;

class NiD3DRenderState : public NiRefObject {
public:
	NiD3DRenderState();
	virtual ~NiD3DRenderState();

	enum NiD3DSamplerState {
		NISAMP_INVALID			= 0xFFFFFFFF,
		NISAMP_ADDRESSU			= 0,
		NISAMP_ADDRESSV			= 1,
		NISAMP_ADDRESSW			= 2,
		NISAMP_BORDERCOLOR		= 3,
		NISAMP_MAGFILTER		= 4,
		NISAMP_MINFILTER		= 5,
		NISAMP_MIPFILTER		= 6,
		NISAMP_MIPMAPLODBIAS	= 7,
		NISAMP_MAXMIPLEVEL		= 8,
		NISAMP_MAXANISOTROPY	= 9,
		NISAMP_SRGBTEXTURE		= 10,
		NISAMP_ELEMENTINDEX		= 11,
		NISAMP_DMAPOFFSET		= 12,
		NISAMP_TOTAL_COUNT		= 13,
	};

	virtual void							UpdateRenderState(const NiPropertyState* pkNew);
	virtual void							ApplyAlpha(const NiAlphaProperty* pkNew);
	virtual void							ApplyDitherMode(const NiDitherProperty* pkNew);
	virtual void							ApplyFog(const NiFogProperty* pkNew);
	virtual void							ApplyMaterial(const NiMaterialProperty* pkNew);
	virtual void							ApplyShading(const NiShadeProperty* pkNew);
	virtual void							ApplySpecular(const NiSpecularProperty* pkNew);
	virtual void							ApplyStencil(const NiStencilProperty* pkNew);
	virtual void							DisableAlpha();
	virtual void							SetBoneCount(uint16_t usNumBones);
	virtual void							vSetModelTransform(const NiTransform* kXform);
	virtual void							SetAntiAlias(bool bOn);
	virtual float							GetMaxFogValue();
	virtual float							GetMaxFogFactor();
	virtual void							SetMaxFogValue(float fFogVal);
	virtual void							GetCameraNearAndFar(float* fNear, float* fFar);
	virtual void							SetCameraNearAndFar(float fNear, float fFar);
	virtual bool							GetLeftHanded();
	virtual void							SetLeftHanded(bool bLeft);
	virtual void							PurgeMaterial(NiMaterialProperty* pkMaterial);
	virtual void							InitRenderState();
	virtual void							SetDestAndSrcBlends(D3DBLEND sourceBlend, D3DBLEND destBlend, int a4, int a5);
	virtual void							SetAlphaBlendEnable(DWORD aValue, uint32_t unk0 = 0, uint32_t unk1 = 0);
	virtual void							SetDepthBias(float afBias, bool abSave = false);
	virtual void							SetRenderState(D3DRENDERSTATETYPE eState, uint32_t uiValue, int, bool bSave);
	virtual uint32_t						GetRenderState(D3DRENDERSTATETYPE eState);
	virtual void							InitShaderState();
	virtual void							SaveShaderState();
	virtual void							RestoreShaderState();
	virtual void							SetPixelShader(LPDIRECT3DPIXELSHADER9 hShader, bool abSave = false);
	virtual LPDIRECT3DPIXELSHADER9			GetPixelShader();
	virtual void							RestorePixelShader();
	virtual void							ClearPixelShader(LPDIRECT3DPIXELSHADER9 hShader);
	virtual void							SetVertexShader(LPDIRECT3DVERTEXSHADER9 hShader, bool abSave = false);
	virtual LPDIRECT3DVERTEXSHADER9			GetVertexShader();
	virtual void							RestoreVertexShader();
	virtual void							ClearVertexShader(LPDIRECT3DVERTEXSHADER9 hShader);
	virtual void							SetFVF(uint32_t uiFVF, bool bSave);
	virtual uint32_t						GetFVF();
	virtual void							RestoreFVF();
	virtual void							ClearFVF(uint32_t uiFVF);
	virtual void							SetDeclaration(LPDIRECT3DVERTEXDECLARATION9 hDeclaration, bool bSave);
	virtual LPDIRECT3DVERTEXDECLARATION9	GetDeclaration();
	virtual void							RestoreDeclaration();
	virtual void							ClearDeclaration(LPDIRECT3DVERTEXDECLARATION9 hDeclaration);
	virtual void							InitTextureStageState();
	virtual void							SetTextureStageState(uint32_t uiStage, D3DTEXTURESTAGESTATETYPE eState, uint32_t uiValue, bool bSave);
	virtual uint32_t						GetTextureStageState(uint32_t uiStage, D3DTEXTURESTAGESTATETYPE eState);
	virtual void							RestoreTextureStageState(uint32_t uiStage, D3DTEXTURESTAGESTATETYPE eState);
	virtual void							SetSamplerState(uint32_t uiStage, D3DSAMPLERSTATETYPE eState, uint32_t uiValue, bool bSave);
	virtual uint32_t						GetSamplerState(uint32_t uiStage, NiD3DSamplerState eState);
	virtual void							RestoreSamplerState(uint32_t uiStage, NiD3DSamplerState eState);
	virtual void							InitTextures();
	virtual void							SetTexture(uint32_t uiStage, LPDIRECT3DBASETEXTURE9 pkTexture);
	virtual LPDIRECT3DBASETEXTURE9			GetTexture(uint32_t uiStage);
	virtual void							ClearTexture(LPDIRECT3DBASETEXTURE9 pkTexture);
	virtual void							SetSoftwareVertexProcessing(bool bSoftware);
	virtual void							GetSoftwareVertexProcessing();
	virtual void							SetForceNormalizeNormals(bool bSet);
	virtual void							GetForceNormalizeNormals();
	virtual void							SetInternalNormalizeNormals(bool bSet);
	virtual void							GetInternalNormalizeNormals();
	virtual void							InitRenderModes();
	virtual void							ReadRenderStateCaps();

	struct NiRenderStateSetting {
		uint32_t m_uiCurrValue;
		uint32_t m_uiPrevValue;
	};

	Bitfield32						m_uiFlags;
	D3DVERTEXBLENDFLAGS				m_aD3DVertexBlendMappings[5];
	D3DBLEND						m_auiAlphaMapping[NiAlphaProperty::ALPHA_MAX_MODES];
	D3DCMPFUNC						m_auiAlphaTestMapping[NiAlphaProperty::TEST_MAX_MODES];
	uint32_t						m_uiSrcAlphaCaps;
	uint32_t						m_uiDestAlphaCaps;
	NiPointer<NiAlphaProperty>		m_spDisabledAlphaProperty;
	float							m_fCameraNear;
	float							m_fCameraFar;
	float							m_fCameraDepthRange;
	float							m_fMaxFogFactor;
	float							m_fMaxFogValue;
	NiColor							m_kCurrentFogColor;
	uint32_t						m_uiCurrentFogColor;
	D3DCMPFUNC						m_auiStencilTestMapping[NiStencilProperty::TEST_MAX];
	D3DSTENCILOP					m_auiStencilActionMapping[NiStencilProperty::ACTION_MAX];
	D3DCULL							m_auiCullModeMapping[NiStencilProperty::DRAW_MAX][2];
	uint32_t						m_uiLeftHanded;
	NiDX9MaterialManager			m_kMaterials;
	D3DCMPFUNC						m_auiZBufferTestMapping[8];
	NiRenderStateSetting			m_akRenderStateSettings[256];
	bool							m_bSavedStates[256];
	NiRenderStateSetting			m_akTextureStageStateSettings[16][8];
	NiRenderStateSetting			m_akSamplerStateSettings[16][5];
	LPDIRECT3DBASETEXTURE9			m_apkTextureStageTextures[16];
	LPDIRECT3DVERTEXSHADER9			m_hCurrentVertexShader;
	LPDIRECT3DVERTEXSHADER9			m_hPreviousVertexShader;
	LPDIRECT3DPIXELSHADER9			m_hCurrentPixelShader;
	LPDIRECT3DPIXELSHADER9			m_hPreviousPixelShader;
	NiDX9ShaderConstantManager*		m_spShaderConstantManager;
	bool							m_bForceNormalizeNormals;
	bool							m_bInternalNormalizeNormals;
	LPDIRECT3DDEVICE9				m_pkD3DDevice;
	NiDX9Renderer*					m_pkD3DRenderer;
};

ASSERT_SIZE(NiD3DRenderState, 0x1100);