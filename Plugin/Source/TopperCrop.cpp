#include "TopperCrop.hpp"

#include "Dependencies.hpp"
#include "Settings.hpp"
#include "StencilTags.hpp"

#include "Gamebryo/NiCamera.hpp"
#include "Gamebryo/NiD3DPixelShader.hpp"
#include "Gamebryo/NiD3DVertexShader.hpp"
#include "Gamebryo/NiDX9Renderer.hpp"

// Crops the lower edge of toppers where they rest on a floor, in the middle of the topper replay.
//
// `CopyBaseDepth` runs before the depth replay. It copies the scene depth, which does not hold the toppers yet, into
// a target of its own. `Crop` runs after the depth replay and a second depth resolve. It draws a full-screen triangle
// over pixels where a topper lies in front of a base. Pixels where the topper lies within a noisy height above the
// floor get the base depth back and lose their topper bit. The colour replay skips them, and the base shows there.
//
// Both passes set device state directly and restore it with a state block. The game's state caches never see a change.
//
// Reference at "Shaders/Toppers/TopperCrop.hlsl".
namespace TopperCrop {

	// Stencil bit marking a base under a topper. Both bits mark a pixel the crop may cut.
	static constexpr uint32_t CROP_BITS = StencilTags::TERRAIN_BLEND_BIT | StencilTags::TOPPER_BIT;

	struct Vertex {
		float x, y, z;
	};

	// Covers the whole clip space square. Every pixel is drawn once.
	static constexpr Vertex kTriangle[3] = {
		{ -1.0f,  1.0f, 0.0f },
		{  3.0f,  1.0f, 0.0f },
		{ -1.0f, -3.0f, 0.0f },
	};

	static NiD3DVertexShader*			pVertexShader	= nullptr;
	static NiD3DPixelShader*			pCopyShader		= nullptr;
	static NiD3DPixelShader*			pCropShader		= nullptr;
	static IDirect3DVertexDeclaration9*	pDeclaration	= nullptr;
	static bool							bReady			= false;

	// Device Resources
	// Released before a device reset and created again on demand.
	static IDirect3DTexture9*			pBaseTexture	= nullptr;	// R32F, reversed depth without toppers
	static IDirect3DSurface9*			pBaseSurface	= nullptr;
	static IDirect3DStateBlock9*		pStateBlock		= nullptr;
	static uint32_t						uiWidth			= 0;
	static uint32_t						uiHeight		= 0;

	template <typename T>
	static void SafeRelease(T*& apObject) {
		if (apObject) {
			apObject->Release();
			apObject = nullptr;
		}
	}

	static void ReleaseResources() {
		SafeRelease(pBaseSurface);
		SafeRelease(pBaseTexture);
		SafeRelease(pStateBlock);
		uiWidth		= 0;
		uiHeight	= 0;
	}

	static bool __cdecl OnDeviceReset(bool abBeforeReset, void*) {
		if (abBeforeReset)
			ReleaseResources();
		return true;
	}

	// Loads the crop shaders and registers for device resets. The crop is off if a shader is missing.
	// Call once the D3D device exists, at NVSE DeferredInit.
	void Initialize() {
		pVertexShader	= Dependencies::CreateVertexShader("IntersectionBlend\\GroundQuad.vso");
		pCopyShader		= Dependencies::CreatePixelShader("IntersectionBlend\\TopperCopy.pso");
		pCropShader		= Dependencies::CreatePixelShader("IntersectionBlend\\TopperCrop.pso");
		if (!pVertexShader || !pCopyShader || !pCropShader) {
			_MESSAGE("Failed to load the topper crop shaders (VS %p, copy %p, crop %p), toppers are not cropped", pVertexShader, pCopyShader, pCropShader);
			return;
		}

		pVertexShader->IncRefCount();
		pCopyShader->IncRefCount();
		pCropShader->IncRefCount();

		NiDX9Renderer* pRenderer = NiDX9Renderer::GetSingleton();
		const D3DVERTEXELEMENT9 kElements[] = {
			{ 0, 0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
			D3DDECL_END()
		};
		if (FAILED(pRenderer->GetD3DDevice()->CreateVertexDeclaration(kElements, &pDeclaration))) {
			_MESSAGE("Failed to create the topper crop vertex declaration, toppers are not cropped");
			return;
		}

		pRenderer->AddResetNotificationFunc(OnDeviceReset, nullptr);
		bReady = true;
	}

	// Creates the base depth target at the size of the given texture if missing or sized differently.
	// Returns false if creation fails.
	static bool CreateResources(IDirect3DDevice9* apDevice, IDirect3DBaseTexture9* apDepthTexture) {
		D3DSURFACE_DESC kDesc;
		if (apDepthTexture->GetType() != D3DRTYPE_TEXTURE || FAILED(static_cast<IDirect3DTexture9*>(apDepthTexture)->GetLevelDesc(0, &kDesc)))
			return false;
		if (pBaseSurface && uiWidth == kDesc.Width && uiHeight == kDesc.Height)
			return true;

		SafeRelease(pBaseSurface);
		SafeRelease(pBaseTexture);

		if ((!pStateBlock && FAILED(apDevice->CreateStateBlock(D3DSBT_ALL, &pStateBlock)))
			|| FAILED(apDevice->CreateTexture(kDesc.Width, kDesc.Height, 1, D3DUSAGE_RENDERTARGET, D3DFMT_R32F, D3DPOOL_DEFAULT, &pBaseTexture, nullptr))
			|| FAILED(pBaseTexture->GetSurfaceLevel(0, &pBaseSurface))) {
			_MESSAGE("Failed to create the topper crop resources (%u x %u R32F), toppers are not cropped", kDesc.Width, kDesc.Height);
			ReleaseResources();
			bReady = false;
			return false;
		}

		uiWidth		= kDesc.Width;
		uiHeight	= kDesc.Height;
		_MESSAGE("Topper crop base depth created (%u x %u)", uiWidth, uiHeight);
		return true;
	}

	static void BindDepthTexture(IDirect3DDevice9* apDevice, uint32_t auiStage, IDirect3DBaseTexture9* apTexture) {
		apDevice->SetTexture(auiStage, apTexture);
		apDevice->SetSamplerState(auiStage, D3DSAMP_MINFILTER, D3DTEXF_POINT);
		apDevice->SetSamplerState(auiStage, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
		apDevice->SetSamplerState(auiStage, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
		apDevice->SetSamplerState(auiStage, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
		apDevice->SetSamplerState(auiStage, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
		apDevice->SetSamplerState(auiStage, D3DSAMP_SRGBTEXTURE, FALSE);
	}

	// Sets the states every full-screen pass of the crop shares, and draws the triangle with the given pixel shader.
	static void DrawTriangle(IDirect3DDevice9* apDevice, NiD3DPixelShader* apPixelShader) {
		apDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
		apDevice->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
		apDevice->SetRenderState(D3DRS_ADAPTIVETESS_Y, 0); // Alpha to coverage, NVIDIA only.
		apDevice->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
		apDevice->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
		apDevice->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);
		apDevice->SetRenderState(D3DRS_SRGBWRITEENABLE, FALSE);
		// The game can leave a user clip plane on after the opaque stage, depending on the view. It would cut the triangle.
		apDevice->SetRenderState(D3DRS_CLIPPLANEENABLE, 0);
		apDevice->SetVertexDeclaration(pDeclaration);
		apDevice->SetVertexShader(pVertexShader->GetShaderHandle());
		apDevice->SetPixelShader(apPixelShader->GetShaderHandle());
		apDevice->DrawPrimitiveUP(D3DPT_TRIANGLELIST, 1, kTriangle, sizeof(Vertex));
	}

	// Copies the given scene depth into the base depth target. Returns false if the crop is unavailable.
	// Call before the depth replay of the toppers, while the scene depth does not hold them.
	bool CopyBaseDepth(IDirect3DDevice9* apDevice, IDirect3DBaseTexture9* apDepthTexture) {
		if (!bReady || !apDepthTexture || Settings::fCropHeight <= 0.0f || !CreateResources(apDevice, apDepthTexture))
			return false;

		IDirect3DSurface9* pSavedTarget = nullptr;
		IDirect3DSurface9* pSavedDepth = nullptr;
		apDevice->GetRenderTarget(0, &pSavedTarget);
		apDevice->GetDepthStencilSurface(&pSavedDepth);
		pStateBlock->Capture();

		const float fScreenParams[4] = { 1.0f / uiWidth, 1.0f / uiHeight, 0.0f, 0.0f };
		apDevice->SetPixelShaderConstantF(5, fScreenParams, 1);

		apDevice->SetRenderTarget(0, pBaseSurface);
		apDevice->SetDepthStencilSurface(nullptr);
		apDevice->SetRenderState(D3DRS_ZENABLE, D3DZB_FALSE);
		apDevice->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
		apDevice->SetRenderState(D3DRS_STENCILENABLE, FALSE);
		apDevice->SetRenderState(D3DRS_COLORWRITEENABLE, D3DCOLORWRITEENABLE_RED);
		BindDepthTexture(apDevice, 0, apDepthTexture);
		DrawTriangle(apDevice, pCopyShader);

		// Render targets are not part of state blocks. Setting them resets the viewport. The state block restores it.
		apDevice->SetRenderTarget(0, pSavedTarget);
		apDevice->SetDepthStencilSurface(pSavedDepth);
		pStateBlock->Apply();
		SafeRelease(pSavedTarget);
		SafeRelease(pSavedDepth);
		return true;
	}

	// Crops toppers on floors: writes the base depth into pixels of the bound target where a topper lies within the crop
	// height above a floor, and clears their topper bit. The given scene depth must hold the toppers.
	// Call between the depth replay and the colour replay, after `CopyBaseDepth`.
	void Crop(IDirect3DDevice9* apDevice, const NiCamera* apCamera, IDirect3DBaseTexture9* apDepthTexture) {
		if (!pBaseTexture || !apCamera || !apDepthTexture)
			return;

		pStateBlock->Capture();

		// The renderer works relative to the camera. Its view matrix carries no translation.
		NiDX9Renderer* pRenderer = NiDX9Renderer::GetSingleton();
		const D3DXMATRIX& rView = *pRenderer->GetD3DView();
		const D3DXMATRIX& rProjection = *pRenderer->GetD3DProj();
		const NiFrustum& kFrustum = apCamera->m_kViewFrustum;
		const NiPoint3& kEye = apCamera->m_kWorld.m_Translate;
		const bool bCause = Settings::eDebugMode == Settings::DEBUG_TOPPER_CROP_CAUSE;
		const float fConstants[8][4] = {
			{ kFrustum.m_fNear, kFrustum.m_fFar - kFrustum.m_fNear, kFrustum.m_fNear * kFrustum.m_fFar, 0.0f },
			{ rProjection._11, rProjection._22, rProjection._31, rProjection._32 },
			{ rView._11, rView._21, rView._31, 0.0f },
			{ rView._12, rView._22, rView._32, 0.0f },
			{ rView._13, rView._23, rView._33, 0.0f },
			{ 1.0f / uiWidth, 1.0f / uiHeight, 0.0f, 0.0f },
			{ kEye.x, kEye.y, kEye.z, 0.0f },
			{ Settings::fCropHeight, Settings::fCropScale, bCause ? 1.0f : 0.0f, 0.0f },
		};
		apDevice->SetPixelShaderConstantF(0, fConstants[0], 8);

		// Only pixels with a topper in front of a base. A cropped pixel loses the topper bit and gets the base depth,
		// whatever the depth buffer holds. Set on the device directly, past LOD Flicker Fix.
		apDevice->SetRenderState(D3DRS_ZENABLE, D3DZB_TRUE);
		apDevice->SetRenderState(D3DRS_ZFUNC, D3DCMP_ALWAYS);
		apDevice->SetRenderState(D3DRS_ZWRITEENABLE, TRUE);
		apDevice->SetRenderState(D3DRS_STENCILENABLE, TRUE);
		apDevice->SetRenderState(D3DRS_TWOSIDEDSTENCILMODE, FALSE);
		apDevice->SetRenderState(D3DRS_STENCILFUNC, D3DCMP_EQUAL);
		apDevice->SetRenderState(D3DRS_STENCILREF, CROP_BITS);
		apDevice->SetRenderState(D3DRS_STENCILMASK, CROP_BITS);
		apDevice->SetRenderState(D3DRS_STENCILWRITEMASK, StencilTags::TOPPER_BIT);
		apDevice->SetRenderState(D3DRS_STENCILPASS, D3DSTENCILOP_INVERT);
		apDevice->SetRenderState(D3DRS_STENCILFAIL, D3DSTENCILOP_KEEP);
		apDevice->SetRenderState(D3DRS_STENCILZFAIL, D3DSTENCILOP_KEEP);
		// Debug mode 9 paints cropped pixels red, mode 10 paints every candidate. The colour replay does not draw over them.
		const bool bTint = Settings::eDebugMode == Settings::DEBUG_TOPPER_CROP || bCause;
		apDevice->SetRenderState(D3DRS_COLORWRITEENABLE, bTint ? D3DCOLORWRITEENABLE_RED | D3DCOLORWRITEENABLE_GREEN | D3DCOLORWRITEENABLE_BLUE : 0);
		BindDepthTexture(apDevice, 0, apDepthTexture);
		BindDepthTexture(apDevice, 1, pBaseTexture);
		DrawTriangle(apDevice, pCropShader);

		pStateBlock->Apply();
	}

}
