#include "OverlayPass.hpp"

#include "Dependencies.hpp"
#include "Settings.hpp"
#include "TerrainPasses.hpp"

#include "Bethesda/BSShaderAccumulator.hpp"
#include "Gamebryo/NiCamera.hpp"
#include "Gamebryo/NiD3DPixelShader.hpp"
#include "Gamebryo/NiD3DVertexShader.hpp"
#include "Gamebryo/NiDX9Renderer.hpp"
#include "Gamebryo/NiDX9RenderState.hpp"

#include <algorithm>
#include <vector>

namespace OverlayPass {
	// Must match shaders/includes/TerrainOverlay.hlsli.
	static constexpr float		OVERLAY_MAGIC				= 7919.0f;
	static constexpr uint32_t	OVERLAY_CONSTANT_REGISTER	= 89;	// c89-c91
	static constexpr uint32_t	OVERLAY_CONSTANT_COUNT		= 3;
	static constexpr uint32_t	SCENE_DEPTH_SAMPLER			= 14;
	static constexpr uint32_t	TERRAIN_SAMPLER_COUNT		= 14;	// BaseMap[7] and NormalMap[7]
	static constexpr const char* OVERLAY_SHADER_MARKER		= "IBOverlayFlags";

	// Offsets used by NiDX9RenderState::SetRenderState (GAME - 0xE88780) and SetTexture (GAME - 0xE88A20).
	ASSERT_OFFSET(NiD3DRenderState, m_akRenderStateSettings, 0x120);
	ASSERT_OFFSET(NiD3DRenderState, m_bSavedStates, 0x920);
	ASSERT_OFFSET(NiD3DRenderState, m_apkTextureStageTextures, 0x10A0);

	struct FullscreenVertex {
		float x, y, z;
		float u, v;
	};

	static NiD3DVertexShader*			pDebugVS				= nullptr;
	static NiD3DPixelShader*			pDebugPS				= nullptr;
	static IDirect3DVertexDeclaration9*	pFullscreenDecl			= nullptr;
	static bool							bDebugReady				= false;
	static bool							bOverlayShadersReady	= false;
	static uint32_t						uiLoggedFrames			= 0;

	// Forces render states at the NiDX9RenderState level for the lifetime of the object, then restores them.
	// NiDX9RenderState::SetRenderState (GAME - 0xE88780) skips every change to a state whose m_bSavedStates
	// entry is set, and its last argument sets that entry. Every render state the game sets goes through it,
	// including Bethesda's BSRenderState layer and LOD Flicker Fix's hook, so the passes drawn meanwhile
	// cannot override the forced values.
	// Never force D3DRS_ZFUNC: LOD Flicker Fix remaps its value on every set, so restoring it would remap it twice.
	class ForcedRenderStates {
	public:
		explicit ForcedRenderStates(NiDX9RenderState* apRenderState) : pRenderState(apRenderState) {}

		~ForcedRenderStates() {
			for (auto it = kSaved.rbegin(); it != kSaved.rend(); ++it) {
				pRenderState->m_bSavedStates[it->eState] = false;
				pRenderState->SetRenderState(it->eState, it->uiValue, 0, false);
				pRenderState->m_bSavedStates[it->eState] = it->bSaved;
			}
		}

		void Force(D3DRENDERSTATETYPE aeState, uint32_t auiValue) {
			kSaved.push_back({ aeState, pRenderState->m_akRenderStateSettings[aeState].m_uiCurrValue, pRenderState->m_bSavedStates[aeState] });
			pRenderState->m_bSavedStates[aeState] = false;
			pRenderState->SetRenderState(aeState, auiValue, 0, true);
		}

	private:
		struct SavedState {
			D3DRENDERSTATETYPE	eState;
			uint32_t			uiValue;
			bool				bSaved;
		};

		NiDX9RenderState*		pRenderState;
		std::vector<SavedState>	kSaved;
	};

	// The overlay needs our terrain shaders. With the original ones the replay would paint plain terrain over everything.
	static bool CheckOverlayShaders() {
		for (uint32_t uiNumber = 2092; uiNumber <= 2146; uiNumber += 2) {
			char cPath[MAX_PATH];
			sprintf_s(cPath, "Data\\Shaders\\Loose\\SLS%u.pso", uiNumber);

			FILE* pFile = nullptr;
			if (fopen_s(&pFile, cPath, "rb") || !pFile) {
				_MESSAGE("Terrain overlay disabled: %s not found", cPath);
				return false;
			}

			std::vector<char> kData;
			char cBuffer[4096];
			size_t uiRead;
			while ((uiRead = fread(cBuffer, 1, sizeof(cBuffer), pFile)) > 0)
				kData.insert(kData.end(), cBuffer, cBuffer + uiRead);
			fclose(pFile);

			const char* pMarkerEnd = OVERLAY_SHADER_MARKER + strlen(OVERLAY_SHADER_MARKER);
			if (std::search(kData.begin(), kData.end(), OVERLAY_SHADER_MARKER, pMarkerEnd) == kData.end()) {
				_MESSAGE("Terrain overlay disabled: %s is not an Intersection Blend terrain shader", cPath);
				return false;
			}
		}

		return true;
	}

	void Initialize() {
		bOverlayShadersReady = CheckOverlayShaders();
		if (bOverlayShadersReady)
			_MESSAGE("Terrain overlay shaders found");

		pDebugVS = Dependencies::CreateVertexShader("IntersectionBlend\\DebugDepth.vso");
		pDebugPS = Dependencies::CreatePixelShader("IntersectionBlend\\DebugDepth.pso");
		if (!pDebugVS || !pDebugPS) {
			_MESSAGE("Failed to load debug shaders (VS %p, PS %p), depth debug view disabled", pDebugVS, pDebugPS);
			return;
		}

		pDebugVS->IncRefCount();
		pDebugPS->IncRefCount();

		const D3DVERTEXELEMENT9 kElements[] = {
			{ 0, 0,  D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
			{ 0, 12, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
			D3DDECL_END()
		};
		if (FAILED(NiDX9Renderer::GetSingleton()->GetD3DDevice()->CreateVertexDeclaration(kElements, &pFullscreenDecl))) {
			_MESSAGE("Failed to create the fullscreen vertex declaration, depth debug view disabled");
			return;
		}

		bDebugReady = true;
	}

	static void GetDepthParams(const NiCamera* apCamera, float(&afParams)[4]) {
		const NiFrustum& kFrustum = apCamera->m_kViewFrustum;
		afParams[0] = kFrustum.m_fNear;
		afParams[1] = kFrustum.m_fFar - kFrustum.m_fNear;
		afParams[2] = kFrustum.m_fNear * kFrustum.m_fFar;
		afParams[3] = 0.0f;
	}

	static uint32_t RenderTerrainOverlay(IDirect3DDevice9* apDevice, const NiCamera* apCamera, const BSRenderedTexture* apTarget, IDirect3DBaseTexture9* apDepthTexture) {
		NiDX9RenderState* pRenderState = NiDX9Renderer::GetSingleton()->m_pkRenderState;

		D3DVIEWPORT9 kViewport;
		apDevice->GetViewport(&kViewport);

		// Nothing in the game uses these registers, so they bypass the game's constant caches.
		float fConstants[OVERLAY_CONSTANT_COUNT][4] = {
			{ OVERLAY_MAGIC, static_cast<float>(Settings::eDebugMode), Settings::fMaxRayGap, 0.0f },
			{},
			{ 1.0f / kViewport.Width, 1.0f / kViewport.Height, Settings::fBlendRange, Settings::fMaxDistance },
		};
		GetDepthParams(apCamera, fConstants[1]);
		apDevice->SetPixelShaderConstantF(OVERLAY_CONSTANT_REGISTER, fConstants[0], OVERLAY_CONSTANT_COUNT);

		// The scene depth sampler is outside the game's texture stage cache too. Save and restore it on the device.
		static constexpr D3DSAMPLERSTATETYPE eSamplerStates[] = { D3DSAMP_MINFILTER, D3DSAMP_MAGFILTER, D3DSAMP_MIPFILTER, D3DSAMP_ADDRESSU, D3DSAMP_ADDRESSV, D3DSAMP_SRGBTEXTURE };
		static constexpr DWORD uiSamplerValues[] = { D3DTEXF_POINT, D3DTEXF_POINT, D3DTEXF_NONE, D3DTADDRESS_CLAMP, D3DTADDRESS_CLAMP, FALSE };
		DWORD uiSavedSamplerValues[ARRAYSIZE(eSamplerStates)];
		IDirect3DBaseTexture9* pSavedTexture = nullptr;
		apDevice->GetTexture(SCENE_DEPTH_SAMPLER, &pSavedTexture);
		for (uint32_t i = 0; i < ARRAYSIZE(eSamplerStates); i++) {
			apDevice->GetSamplerState(SCENE_DEPTH_SAMPLER, eSamplerStates[i], &uiSavedSamplerValues[i]);
			apDevice->SetSamplerState(SCENE_DEPTH_SAMPLER, eSamplerStates[i], uiSamplerValues[i]);
		}
		apDevice->SetTexture(SCENE_DEPTH_SAMPLER, apDepthTexture);

		// Depth Resolve's RESZ resolve binds the depth texture to stage 0 without going through the game's texture cache.
		// Clear the cached terrain stages so every replayed pass binds its own textures.
		for (uint32_t i = 0; i < TERRAIN_SAMPLER_COUNT; i++)
			pRenderState->SetTexture(i, nullptr);

		uint32_t uiPasses = 0;
		{
			ForcedRenderStates kStates(pRenderState);
			kStates.Force(D3DRS_ZENABLE, D3DZB_FALSE);
			kStates.Force(D3DRS_ZWRITEENABLE, FALSE);
			kStates.Force(D3DRS_STENCILENABLE, FALSE);
			kStates.Force(D3DRS_ALPHATESTENABLE, FALSE);
			kStates.Force(D3DRS_ADAPTIVETESS_Y, 0); // Alpha to coverage, NVIDIA only.
			kStates.Force(D3DRS_ALPHABLENDENABLE, TRUE);
			kStates.Force(D3DRS_SEPARATEALPHABLENDENABLE, FALSE);
			kStates.Force(D3DRS_BLENDOP, D3DBLENDOP_ADD);
			kStates.Force(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
			kStates.Force(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
			kStates.Force(D3DRS_COLORWRITEENABLE, D3DCOLORWRITEENABLE_RED | D3DCOLORWRITEENABLE_GREEN | D3DCOLORWRITEENABLE_BLUE);

			uiPasses = TerrainPasses::Replay(apTarget);
		}

		const float fZero[OVERLAY_CONSTANT_COUNT][4] = {};
		apDevice->SetPixelShaderConstantF(OVERLAY_CONSTANT_REGISTER, fZero[0], OVERLAY_CONSTANT_COUNT);

		apDevice->SetTexture(SCENE_DEPTH_SAMPLER, pSavedTexture);
		if (pSavedTexture)
			pSavedTexture->Release();
		for (uint32_t i = 0; i < ARRAYSIZE(eSamplerStates); i++)
			apDevice->SetSamplerState(SCENE_DEPTH_SAMPLER, eSamplerStates[i], uiSavedSamplerValues[i]);

		return uiPasses;
	}

	static void DrawDebugDepth(IDirect3DDevice9* apDevice, const NiCamera* apCamera, IDirect3DBaseTexture9* apDepthTexture) {
		// Capture everything we touch so the game's render state cache stays coherent.
		IDirect3DStateBlock9* pStateBlock = nullptr;
		if (FAILED(apDevice->CreateStateBlock(D3DSBT_ALL, &pStateBlock)))
			return;

		D3DVIEWPORT9 kViewport;
		apDevice->GetViewport(&kViewport);

		float fDepthParams[4];
		GetDepthParams(apCamera, fDepthParams);
		const float fBlendParams[4] = { Settings::fMaxDistance, Settings::fBlendRange, static_cast<float>(Settings::eDebugMode), 0.0f };
		const float fTexelSize[4]	= { 1.0f / kViewport.Width, 1.0f / kViewport.Height, 0.0f, 0.0f };

		apDevice->SetVertexShaderConstantF(0, fTexelSize, 1);
		apDevice->SetPixelShaderConstantF(0, fDepthParams, 1);
		apDevice->SetPixelShaderConstantF(1, fBlendParams, 1);

		apDevice->SetRenderState(D3DRS_ZENABLE, D3DZB_FALSE);
		apDevice->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
		apDevice->SetRenderState(D3DRS_STENCILENABLE, FALSE);
		apDevice->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
		apDevice->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
		apDevice->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
		apDevice->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);
		apDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
		apDevice->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD);
		apDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
		apDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
		apDevice->SetRenderState(D3DRS_COLORWRITEENABLE, D3DCOLORWRITEENABLE_RED | D3DCOLORWRITEENABLE_GREEN | D3DCOLORWRITEENABLE_BLUE);

		apDevice->SetTexture(0, apDepthTexture);
		apDevice->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_POINT);
		apDevice->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
		apDevice->SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
		apDevice->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
		apDevice->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
		apDevice->SetSamplerState(0, D3DSAMP_SRGBTEXTURE, FALSE);

		apDevice->SetVertexDeclaration(pFullscreenDecl);
		apDevice->SetVertexShader(pDebugVS->GetShaderHandle());
		apDevice->SetPixelShader(pDebugPS->GetShaderHandle());

		const FullscreenVertex kQuad[4] = {
			{ -1.0f,  1.0f, 0.0f, 0.0f, 0.0f },
			{  1.0f,  1.0f, 0.0f, 1.0f, 0.0f },
			{ -1.0f, -1.0f, 0.0f, 0.0f, 1.0f },
			{  1.0f, -1.0f, 0.0f, 1.0f, 1.0f },
		};
		apDevice->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, kQuad, sizeof(FullscreenVertex));

		pStateBlock->Apply();
		pStateBlock->Release();
	}

	void __cdecl OnPostResolveDepth(BSShaderAccumulator* apAccumulator, BSRenderedTexture* apTarget, IDirect3DBaseTexture9* apDepthTexture) {
		if (!Settings::bEnabled || !apAccumulator || !apAccumulator->m_pkCamera || !apDepthTexture) {
			TerrainPasses::Clear();
			return;
		}

		IDirect3DDevice9* pDevice = NiDX9Renderer::GetSingleton()->GetD3DDevice();

		uint32_t uiPasses = 0;
		if (bOverlayShadersReady)
			uiPasses = RenderTerrainOverlay(pDevice, apAccumulator->m_pkCamera, apTarget, apDepthTexture);
		TerrainPasses::Clear();

		if (bDebugReady && Settings::eDebugMode == Settings::DEBUG_DEPTH_BANDS)
			DrawDebugDepth(pDevice, apAccumulator->m_pkCamera, apDepthTexture);

		if (uiLoggedFrames < 3) {
			const NiFrustum& kFrustum = apAccumulator->m_pkCamera->m_kViewFrustum;
			_MESSAGE("Post-resolve callback: target %p, depth %p, near %.2f, far %.2f, %u landscape passes replayed", apTarget, apDepthTexture, kFrustum.m_fNear, kFrustum.m_fFar, uiPasses);
			uiLoggedFrames++;
		}
	}
}
