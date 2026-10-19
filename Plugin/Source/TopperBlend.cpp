#include "TopperBlend.hpp"

#include "Dependencies.hpp"
#include "Hotkeys.hpp"
#include "Settings.hpp"
#include "StencilTags.hpp"

#include "Gamebryo/NiCamera.hpp"
#include "Gamebryo/NiD3DPixelShader.hpp"
#include "Gamebryo/NiD3DVertexShader.hpp"
#include "Gamebryo/NiDX9Renderer.hpp"

// Fades the foot of toppers into the floor they rest on, around the topper replay.
//
// `CopyBase` runs before the depth replay. It copies the scene depth and the scene colour, which do not hold the
// toppers yet. `MarkBases` runs after the depth replay and sets `TOPPER_BASE_BIT` where a topper lies in front of a
// tagged base. Between `BeginFeatures` and `EndFeatures`, after the colour replay, the topper passes draw a third time
// with the topper feature shader into a feature target. `Composite` then blends the copied base colour over the
// topper's foot over the static blend range above a floor. It shapes the edge with the features and adds the contact
// shadow, like the terrain overlay. The toppers keep their depth. AO and the ground pass see them whole.
//
// All passes set device state directly and restore it with a state block. The game's state caches never see a change.
//
// Reference at "Shaders/Toppers/TopperBlend.hlsl".
namespace TopperBlend {

	// A topper in front of a base. The colour replay replaces the base's `TERRAIN_BLEND_BIT`, so the composite tests
	// `TOPPER_BASE_BIT` instead.
	static constexpr uint32_t BASE_BITS = StencilTags::TERRAIN_BLEND_BIT | StencilTags::TOPPER_BIT;
	static constexpr uint32_t COMPOSITE_BITS = StencilTags::TOPPER_BASE_BIT | StencilTags::TOPPER_BIT;

	struct Vertex {
		float x, y, z;
	};

	// Covers the whole clip space square. Every pixel is drawn once.
	static constexpr Vertex kTriangle[3] = {
		{ -1.0f, 1.0f, 0.0f },
		{ 3.0f, 1.0f, 0.0f },
		{ -1.0f, -3.0f, 0.0f },
	};

	static NiD3DVertexShader* pVertexShader = nullptr;
	static NiD3DPixelShader* pCopyShader = nullptr;
	static NiD3DPixelShader* pCompositeShader = nullptr;
	static NiD3DPixelShader* pFeatureShader = nullptr;
	static IDirect3DVertexDeclaration9* pDeclaration = nullptr;
	static bool bReady = false;

	// Device Resources
	// Released before a device reset and created again on demand.
	static IDirect3DTexture9* pBaseDepthTexture = nullptr; // R32F, reversed depth without toppers
	static IDirect3DSurface9* pBaseDepthSurface = nullptr;
	static IDirect3DTexture9* pBaseColorTexture = nullptr; // Format of the scene target, without toppers
	static IDirect3DSurface9* pBaseColorSurface = nullptr;
	static IDirect3DStateBlock9* pStateBlock = nullptr;
	static uint32_t uiWidth = 0;
	static uint32_t uiHeight = 0;
	static D3DFORMAT eColorFormat = D3DFMT_UNKNOWN;

	// Texture features of the toppers, 0.5 + features / 2 in red. The target matches the multisampling of the scene's
	// depth-stencil surface, whose stencil the feature replay tests. It is resolved into the texture for the composite.
	static IDirect3DSurface9* pFeatureTarget = nullptr;
	static IDirect3DTexture9* pFeatureTexture = nullptr; // A8R8G8B8
	static IDirect3DSurface9* pFeatureSurface = nullptr;
	static D3DMULTISAMPLE_TYPE eFeatureMultisample = D3DMULTISAMPLE_NONE;
	static bool bFeaturesReady = false; // The feature texture holds this frame's features.

	// Feature replay state, kept from `BeginFeatures` to `EndFeatures`
	static IDirect3DSurface9* pFeatureSavedTarget = nullptr;
	static D3DVIEWPORT9 kFeatureSavedViewport;

	template <typename T>
	static void SafeRelease(T*& apObject) {
		if (apObject) {
			apObject->Release();
			apObject = nullptr;
		}
	}

	static void ReleaseFeatureResources() {
		SafeRelease(pFeatureTarget);
		SafeRelease(pFeatureSurface);
		SafeRelease(pFeatureTexture);
		eFeatureMultisample = D3DMULTISAMPLE_NONE;
		bFeaturesReady = false;
	}

	static void ReleaseResources() {
		ReleaseFeatureResources();
		SafeRelease(pBaseDepthSurface);
		SafeRelease(pBaseDepthTexture);
		SafeRelease(pBaseColorSurface);
		SafeRelease(pBaseColorTexture);
		SafeRelease(pStateBlock);
		uiWidth = 0;
		uiHeight = 0;
		eColorFormat = D3DFMT_UNKNOWN;
	}

	static bool __cdecl OnDeviceReset(bool abBeforeReset, void*) {
		if (abBeforeReset) {
			ReleaseResources();
		}

		return true;
	}

	// Loads the topper blend shaders and registers for device resets. The blend is off if a shader is missing.
	// Call once the D3D device exists, at NVSE DeferredInit.
	void Initialize() {
		pVertexShader = Dependencies::CreateVertexShader("IntersectionBlend\\GroundQuad.vso");
		pCopyShader = Dependencies::CreatePixelShader("IntersectionBlend\\TopperCopy.pso");
		pCompositeShader = Dependencies::CreatePixelShader("IntersectionBlend\\TopperComposite.pso");
		pFeatureShader = Dependencies::CreatePixelShader("IntersectionBlend\\TopperFeatures.pso");

		if (pFeatureShader) {
			pFeatureShader->IncRefCount();
		}
		else {
			_MESSAGE("Failed to load the topper feature shader, the topper blend has no texture features");
		}

		if (!pVertexShader || !pCopyShader || !pCompositeShader) {
			_MESSAGE("Failed to load the topper blend shaders (VS %p, copy %p, composite %p), toppers do not blend", pVertexShader, pCopyShader, pCompositeShader);
			return;
		}

		pVertexShader->IncRefCount();
		pCopyShader->IncRefCount();
		pCompositeShader->IncRefCount();

		NiDX9Renderer* pRenderer = NiDX9Renderer::GetSingleton();
		const D3DVERTEXELEMENT9 kElements[] = {
			{ 0, 0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
			D3DDECL_END()
		};

		if (FAILED(pRenderer->GetD3DDevice()->CreateVertexDeclaration(kElements, &pDeclaration))) {
			_MESSAGE("Failed to create the topper blend vertex declaration, toppers do not blend");
			return;
		}

		pRenderer->AddResetNotificationFunc(OnDeviceReset, nullptr);
		bReady = true;
	}

	// Creates the base depth and colour targets at the size of the given depth texture and in the format of the given
	// scene target, if missing or different. Returns false if creation fails.
	static bool CreateResources(IDirect3DDevice9* apDevice, IDirect3DBaseTexture9* apDepthTexture, IDirect3DSurface9* apSceneTarget) {
		D3DSURFACE_DESC kDepthDesc;
		D3DSURFACE_DESC kSceneDesc;

		if (apDepthTexture->GetType() != D3DRTYPE_TEXTURE || FAILED(static_cast<IDirect3DTexture9*>(apDepthTexture)->GetLevelDesc(0, &kDepthDesc))
			|| FAILED(apSceneTarget->GetDesc(&kSceneDesc))) {
			return false;
		}

		if (pBaseDepthSurface && uiWidth == kDepthDesc.Width && uiHeight == kDepthDesc.Height && eColorFormat == kSceneDesc.Format) {
			return true;
		}

		SafeRelease(pBaseDepthSurface);
		SafeRelease(pBaseDepthTexture);
		SafeRelease(pBaseColorSurface);
		SafeRelease(pBaseColorTexture);

		if ((!pStateBlock && FAILED(apDevice->CreateStateBlock(D3DSBT_ALL, &pStateBlock)))
			|| FAILED(apDevice->CreateTexture(kDepthDesc.Width, kDepthDesc.Height, 1, D3DUSAGE_RENDERTARGET, D3DFMT_R32F, D3DPOOL_DEFAULT, &pBaseDepthTexture, nullptr))
			|| FAILED(pBaseDepthTexture->GetSurfaceLevel(0, &pBaseDepthSurface))
			|| FAILED(apDevice->CreateTexture(kDepthDesc.Width, kDepthDesc.Height, 1, D3DUSAGE_RENDERTARGET, kSceneDesc.Format, D3DPOOL_DEFAULT, &pBaseColorTexture, nullptr))
			|| FAILED(pBaseColorTexture->GetSurfaceLevel(0, &pBaseColorSurface))) {
			_MESSAGE("Failed to create the topper blend resources (%u x %u R32F and format %u), toppers do not blend", kDepthDesc.Width, kDepthDesc.Height, kSceneDesc.Format);
			ReleaseResources();
			bReady = false;
			return false;
		}

		uiWidth = kDepthDesc.Width;
		uiHeight = kDepthDesc.Height;
		eColorFormat = kSceneDesc.Format;
		_MESSAGE("Topper blend base copies created (%u x %u, scene format %u, multisample type %u)", uiWidth, uiHeight, kSceneDesc.Format, kSceneDesc.MultiSampleType);
		return true;
	}

	static void BindTexture(IDirect3DDevice9* apDevice, uint32_t auiStage, IDirect3DBaseTexture9* apTexture) {
		apDevice->SetTexture(auiStage, apTexture);
		apDevice->SetSamplerState(auiStage, D3DSAMP_MINFILTER, D3DTEXF_POINT);
		apDevice->SetSamplerState(auiStage, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
		apDevice->SetSamplerState(auiStage, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
		apDevice->SetSamplerState(auiStage, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
		apDevice->SetSamplerState(auiStage, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
		apDevice->SetSamplerState(auiStage, D3DSAMP_SRGBTEXTURE, FALSE);
	}

	// Sets the states every full-screen pass of the topper blend shares, and draws the triangle with the given pixel
	// shader. Blending stays as the caller set it.
	static void DrawTriangle(IDirect3DDevice9* apDevice, NiD3DPixelShader* apPixelShader) {
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

	// Copies the given scene depth and the colour of the bound scene target into the base targets. Returns false if the
	// topper blend is unavailable.
	// Call before the depth replay of the toppers, while neither holds them.
	bool CopyBase(IDirect3DDevice9* apDevice, IDirect3DBaseTexture9* apDepthTexture) {
		if (!bReady || !Hotkeys::IsBlendActive() || !apDepthTexture || Settings::fStaticBlendRange <= 0.0f) {
			return false;
		}

		IDirect3DSurface9* pSavedTarget = nullptr;
		IDirect3DSurface9* pSavedDepth = nullptr;
		apDevice->GetRenderTarget(0, &pSavedTarget);
		apDevice->GetDepthStencilSurface(&pSavedDepth);
		const bool bCreated = pSavedTarget && CreateResources(apDevice, apDepthTexture, pSavedTarget);

		if (bCreated) {
			// Resolves the scene target if it is multisampled.
			apDevice->StretchRect(pSavedTarget, nullptr, pBaseColorSurface, nullptr, D3DTEXF_NONE);

			pStateBlock->Capture();
			const float fScreenParams[4] = { 1.0f / uiWidth, 1.0f / uiHeight, 0.0f, 0.0f };
			apDevice->SetPixelShaderConstantF(5, fScreenParams, 1);

			apDevice->SetRenderTarget(0, pBaseDepthSurface);
			apDevice->SetDepthStencilSurface(nullptr);
			apDevice->SetRenderState(D3DRS_ZENABLE, D3DZB_FALSE);
			apDevice->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
			apDevice->SetRenderState(D3DRS_STENCILENABLE, FALSE);
			apDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
			apDevice->SetRenderState(D3DRS_COLORWRITEENABLE, D3DCOLORWRITEENABLE_RED);
			BindTexture(apDevice, 0, apDepthTexture);
			DrawTriangle(apDevice, pCopyShader);

			// Render targets are not part of state blocks. Setting them resets the viewport. The state block restores it.
			apDevice->SetRenderTarget(0, pSavedTarget);
			apDevice->SetDepthStencilSurface(pSavedDepth);
			pStateBlock->Apply();
		}
		SafeRelease(pSavedTarget);
		SafeRelease(pSavedDepth);
		return bCreated;
	}

	// Sets `TOPPER_BASE_BIT` where a topper lies in front of a tagged base. Writes nothing else.
	// Call after the depth replay of the toppers and before the colour replay, while `TERRAIN_BLEND_BIT` still marks the
	// base under each topper pixel.
	void MarkBases(IDirect3DDevice9* apDevice) {
		pStateBlock->Capture();

		apDevice->SetRenderState(D3DRS_ZENABLE, D3DZB_FALSE);
		apDevice->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
		apDevice->SetRenderState(D3DRS_STENCILENABLE, TRUE);
		apDevice->SetRenderState(D3DRS_TWOSIDEDSTENCILMODE, FALSE);
		apDevice->SetRenderState(D3DRS_STENCILFUNC, D3DCMP_EQUAL);
		apDevice->SetRenderState(D3DRS_STENCILREF, BASE_BITS | StencilTags::TOPPER_BASE_BIT);
		apDevice->SetRenderState(D3DRS_STENCILMASK, BASE_BITS);
		apDevice->SetRenderState(D3DRS_STENCILWRITEMASK, StencilTags::TOPPER_BASE_BIT);
		apDevice->SetRenderState(D3DRS_STENCILPASS, D3DSTENCILOP_REPLACE);
		apDevice->SetRenderState(D3DRS_STENCILFAIL, D3DSTENCILOP_KEEP);
		apDevice->SetRenderState(D3DRS_STENCILZFAIL, D3DSTENCILOP_KEEP);
		apDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
		apDevice->SetRenderState(D3DRS_COLORWRITEENABLE, 0);
		DrawTriangle(apDevice, pCopyShader);

		pStateBlock->Apply();
	}

	// Creates the feature target and texture at the base size, with the multisampling of the given depth-stencil surface,
	// if missing or different. Returns false if creation fails.
	static bool CreateFeatureResources(IDirect3DDevice9* apDevice, IDirect3DSurface9* apDepthStencil) {
		D3DSURFACE_DESC kDesc;

		if (!apDepthStencil || FAILED(apDepthStencil->GetDesc(&kDesc))) {
			return false;
		}

		if (pFeatureTarget && eFeatureMultisample == kDesc.MultiSampleType) {
			return true;
		}

		ReleaseFeatureResources();

		if (FAILED(apDevice->CreateRenderTarget(uiWidth, uiHeight, D3DFMT_A8R8G8B8, kDesc.MultiSampleType, kDesc.MultiSampleQuality, FALSE, &pFeatureTarget, nullptr))
			|| FAILED(apDevice->CreateTexture(uiWidth, uiHeight, 1, D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &pFeatureTexture, nullptr))
			|| FAILED(pFeatureTexture->GetSurfaceLevel(0, &pFeatureSurface))) {
			_MESSAGE("Failed to create the topper feature target (%u x %u, multisample type %u), the topper blend has no texture features", uiWidth, uiHeight, kDesc.MultiSampleType);
			ReleaseFeatureResources();
			return false;
		}

		eFeatureMultisample = kDesc.MultiSampleType;
		_MESSAGE("Topper feature target created (%u x %u, multisample type %u)", uiWidth, uiHeight, kDesc.MultiSampleType);
		return true;
	}

	// Binds the cleared feature target in place of the scene target and returns the topper feature shader, or null if
	// texture features are unavailable. The caller replays the topper passes with that shader, then calls `EndFeatures`.
	// Call after `CopyBase` and the colour replay, with the scene's depth-stencil surface bound.
	IDirect3DPixelShader9* BeginFeatures(IDirect3DDevice9* apDevice) {
		bFeaturesReady = false;

		if (!pFeatureShader || !pBaseDepthTexture) {
			return nullptr;
		}

		IDirect3DSurface9* pDepthStencil = nullptr;
		apDevice->GetDepthStencilSurface(&pDepthStencil);
		const bool bCreated = CreateFeatureResources(apDevice, pDepthStencil);
		SafeRelease(pDepthStencil);

		if (!bCreated) {
			return nullptr;
		}

		apDevice->GetRenderTarget(0, &pFeatureSavedTarget);
		apDevice->GetViewport(&kFeatureSavedViewport);
		apDevice->SetRenderTarget(0, pFeatureTarget);

		// Clears honour the scissor rectangle. Neutral features where no topper draws.
		DWORD uiScissor = FALSE;
		apDevice->GetRenderState(D3DRS_SCISSORTESTENABLE, &uiScissor);
		apDevice->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
		apDevice->Clear(0, nullptr, D3DCLEAR_TARGET, D3DCOLOR_ARGB(255, 128, 0, 0), 1.0f, 0);
		apDevice->SetRenderState(D3DRS_SCISSORTESTENABLE, uiScissor);
		return pFeatureShader->GetShaderHandle();
	}

	// Binds the scene target again and resolves the features into the feature texture.
	void EndFeatures(IDirect3DDevice9* apDevice) {
		if (!pFeatureSavedTarget) {
			return;
		}

		// Setting a render target resets the viewport.
		apDevice->SetRenderTarget(0, pFeatureSavedTarget);
		apDevice->SetViewport(&kFeatureSavedViewport);
		SafeRelease(pFeatureSavedTarget);
		bFeaturesReady = SUCCEEDED(apDevice->StretchRect(pFeatureTarget, nullptr, pFeatureSurface, nullptr, D3DTEXF_NONE));
	}

	// Blends the base colour over the foot of toppers on floors, with the contact shadow on the topper above it.
	// The given scene depth must hold the toppers.
	// Call after the colour replay of the toppers, after `CopyBase` and `MarkBases`.
	void Composite(IDirect3DDevice9* apDevice, const NiCamera* apCamera, IDirect3DBaseTexture9* apDepthTexture) {
		if (!pBaseDepthTexture || !apCamera || !apDepthTexture) {
			return;
		}

		pStateBlock->Capture();

		// The renderer works relative to the camera. Its view matrix carries no translation.
		NiDX9Renderer* pRenderer = NiDX9Renderer::GetSingleton();
		const D3DXMATRIX& rView = *pRenderer->GetD3DView();
		const D3DXMATRIX& rProjection = *pRenderer->GetD3DProj();
		const NiFrustum& kFrustum = apCamera->m_kViewFrustum;
		const NiPoint3& kEye = apCamera->m_kWorld.m_Translate;
		// Debug mode 9 or 10 for the composite, else 0.
		float fDebugMode = 0.0f;

		if (Settings::eDebugMode == Settings::DEBUG_TOPPER_BLEND || Settings::eDebugMode == Settings::DEBUG_TOPPER_BLEND_CAUSE) {
			fDebugMode = static_cast<float>(Settings::eDebugMode);
		}

		float fFeatureStrength = 0.0f;

		if (bFeaturesReady) {
			fFeatureStrength = Settings::fEdgeFeatureStrength;
		}

		float fFeatureView = 0.0f;

		if (Settings::eDebugMode == Settings::DEBUG_EDGE_FEATURES) {
			fFeatureView = 1.0f;
		}

		const float fConstants[9][4] = {
			{ kFrustum.m_fNear, kFrustum.m_fFar - kFrustum.m_fNear, kFrustum.m_fNear * kFrustum.m_fFar, 0.0f },
			{ rProjection._11, rProjection._22, rProjection._31, rProjection._32 },
			{ rView._11, rView._21, rView._31, 0.0f },
			{ rView._12, rView._22, rView._32, 0.0f },
			{ rView._13, rView._23, rView._33, 0.0f },
			{ 1.0f / uiWidth, 1.0f / uiHeight, 0.0f, 0.0f },
			{ kEye.x, kEye.y, kEye.z, 0.0f },
			{ Settings::fStaticBlendRange, Settings::fEdgeSoftness, fDebugMode, Settings::fContactShadow * Settings::fStaticContactShadowFactor },
			{ Settings::fContactShadowWidth, fFeatureStrength, fFeatureView, 0.0f },
		};

		apDevice->SetPixelShaderConstantF(0, fConstants[0], 9);

		// Only topper pixels in front of a tagged base. Set on the device directly, past LOD Flicker Fix.
		apDevice->SetRenderState(D3DRS_ZENABLE, D3DZB_FALSE);
		apDevice->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
		apDevice->SetRenderState(D3DRS_STENCILENABLE, TRUE);
		apDevice->SetRenderState(D3DRS_TWOSIDEDSTENCILMODE, FALSE);
		apDevice->SetRenderState(D3DRS_STENCILFUNC, D3DCMP_EQUAL);
		apDevice->SetRenderState(D3DRS_STENCILREF, COMPOSITE_BITS);
		apDevice->SetRenderState(D3DRS_STENCILMASK, COMPOSITE_BITS);
		apDevice->SetRenderState(D3DRS_STENCILWRITEMASK, 0);
		apDevice->SetRenderState(D3DRS_STENCILPASS, D3DSTENCILOP_KEEP);
		apDevice->SetRenderState(D3DRS_STENCILFAIL, D3DSTENCILOP_KEEP);
		apDevice->SetRenderState(D3DRS_STENCILZFAIL, D3DSTENCILOP_KEEP);
		apDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
		apDevice->SetRenderState(D3DRS_SEPARATEALPHABLENDENABLE, FALSE);
		apDevice->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD);
		apDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
		apDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
		apDevice->SetRenderState(D3DRS_COLORWRITEENABLE, D3DCOLORWRITEENABLE_RED | D3DCOLORWRITEENABLE_GREEN | D3DCOLORWRITEENABLE_BLUE);
		BindTexture(apDevice, 0, apDepthTexture);
		BindTexture(apDevice, 1, pBaseDepthTexture);
		BindTexture(apDevice, 2, pBaseColorTexture);
		IDirect3DTexture9* pFeatures = nullptr;

		if (bFeaturesReady) {
			pFeatures = pFeatureTexture;
		}

		BindTexture(apDevice, 3, pFeatures);
		DrawTriangle(apDevice, pCompositeShader);
		bFeaturesReady = false;

		pStateBlock->Apply();
	}

}
