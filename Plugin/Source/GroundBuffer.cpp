#include "GroundBuffer.hpp"

#include "Dependencies.hpp"
#include "RenderStates.hpp"
#include "Settings.hpp"
#include "TerrainPasses.hpp"

#include "Gamebryo/NiCamera.hpp"
#include "Gamebryo/NiD3DPixelShader.hpp"
#include "Gamebryo/NiD3DVertexShader.hpp"
#include "Gamebryo/NiDX9Renderer.hpp"
#include "Gamebryo/NiGeometry.hpp"

#include <unordered_map>

// LOD Flicker Fix patches m_kD3DProj._33 and _43 at +0x9E8 and +0x9F8 (GAME - 0xE6CBB7).
ASSERT_OFFSET(NiDX9Renderer, m_kD3DProj, 0x9C0);

namespace GroundBuffer {
	// Height field: FIELD_SIZE² texels over FIELD_EXTENT² game units around the camera (8 units per texel by default,
	// terrain vertices are 128 units apart). Its camera sits FIELD_DEPTH / 2 above the camera and sees FIELD_DEPTH down.
	static constexpr uint32_t	FIELD_SIZE				= 1024;
	static constexpr float		FIELD_DEPTH				= 65536.0f;
	static constexpr float		FIELD_RECENTER_DISTANCE	= 512.0f;
	static constexpr uint32_t	TERRAIN_SAMPLER_COUNT	= 14;	// BaseMap[7] and NormalMap[7]

	// NiRenderer::SetCameraData(location, direction, up, right, frustum, port) (GAME - 0x4E9C90). Checks that a frame
	// is in progress, then calls NiDX9Renderer::Do_SetCameraData through the vtable.
	static constexpr uint32_t	SET_CAMERA_DATA			= 0x4E9C90;

	struct FullscreenVertex {
		float x, y, z;
		float u, v;
	};

	static NiD3DVertexShader*			pVertexShader		= nullptr;
	static NiD3DPixelShader*			pPixelShader		= nullptr;
	static IDirect3DVertexDeclaration9*	pDeclaration		= nullptr;
	static bool							bReady				= false;

	// Device resources, released before a device reset and created again on demand.
	static IDirect3DTexture9*			pFieldTexture		= nullptr;	// INTZ
	static IDirect3DSurface9*			pFieldDepth			= nullptr;
	static IDirect3DSurface9*			pFieldColor			= nullptr;	// D3D9 needs a colour target, nothing is written to it.
	static IDirect3DTexture9*			pGroundTexture		= nullptr;
	static IDirect3DSurface9*			pGroundSurface		= nullptr;
	static IDirect3DStateBlock9*		pStateBlock			= nullptr;
	static uint32_t						uiGroundWidth		= 0;
	static uint32_t						uiGroundHeight		= 0;

	// Height field contents.
	static bool							bFieldValid			= false;
	static NiPoint3						kFieldCenter;
	static D3DXMATRIX					kFieldViewProj;
	// Landscape geometry drawn into the height field, with its bound centre then. Geometry gets freed when cells
	// unload and its address reused, so a block only counts as drawn while it is still in the same place.
	static std::unordered_map<const NiGeometry*, NiPoint3> kFieldGeometry;

	static bool IsInField(const NiGeometry* apGeometry) {
		const auto it = kFieldGeometry.find(apGeometry);
		const NiBound* pBound = apGeometry->m_pWorldBound;
		return it != kFieldGeometry.end() && pBound
			&& it->second.x == pBound->m_kCenter.x && it->second.y == pBound->m_kCenter.y && it->second.z == pBound->m_kCenter.z;
	}

	// Records a geometry as drawn into the height field. Returns false if it already was.
	static bool AddToField(const NiGeometry* apGeometry) {
		if (IsInField(apGeometry))
			return false;
		kFieldGeometry[apGeometry] = apGeometry->m_pWorldBound ? apGeometry->m_pWorldBound->m_kCenter : NiPoint3();
		return true;
	}

	template <typename T>
	static void SafeRelease(T*& apObject) {
		if (apObject) {
			apObject->Release();
			apObject = nullptr;
		}
	}

	static void ReleaseResources() {
		SafeRelease(pFieldDepth);
		SafeRelease(pFieldTexture);
		SafeRelease(pFieldColor);
		SafeRelease(pGroundSurface);
		SafeRelease(pGroundTexture);
		SafeRelease(pStateBlock);
		uiGroundWidth	= 0;
		uiGroundHeight	= 0;
		bFieldValid		= false;
	}

	static bool __cdecl OnDeviceReset(bool abBeforeReset, void*) {
		if (abBeforeReset)
			ReleaseResources();
		return true;
	}

	static bool CreateFieldResources(IDirect3DDevice9* apDevice) {
		if (pFieldDepth)
			return true;

		if (FAILED(apDevice->CreateTexture(FIELD_SIZE, FIELD_SIZE, 1, D3DUSAGE_DEPTHSTENCIL, static_cast<D3DFORMAT>(MAKEFOURCC('I', 'N', 'T', 'Z')), D3DPOOL_DEFAULT, &pFieldTexture, nullptr))
			|| FAILED(pFieldTexture->GetSurfaceLevel(0, &pFieldDepth))
			|| FAILED(apDevice->CreateRenderTarget(FIELD_SIZE, FIELD_SIZE, D3DFMT_R5G6B5, D3DMULTISAMPLE_NONE, 0, FALSE, &pFieldColor, nullptr))) {
			_MESSAGE("Failed to create the height field (%u x %u INTZ), terrain overlay disabled", FIELD_SIZE, FIELD_SIZE);
			ReleaseResources();
			bReady = false;
			return false;
		}

		_MESSAGE("Height field created (%u x %u over %.0f units)", FIELD_SIZE, FIELD_SIZE, Settings::fMaxDistance * 2.0f);
		return true;
	}

	static bool CreateGroundResources(IDirect3DDevice9* apDevice, uint32_t auiWidth, uint32_t auiHeight) {
		if (pGroundSurface && uiGroundWidth == auiWidth && uiGroundHeight == auiHeight)
			return true;

		SafeRelease(pGroundSurface);
		SafeRelease(pGroundTexture);

		if (!pStateBlock && FAILED(apDevice->CreateStateBlock(D3DSBT_ALL, &pStateBlock))) {
			_MESSAGE("Failed to create the ground buffer state block, terrain overlay disabled");
			bReady = false;
			return false;
		}

		if (FAILED(apDevice->CreateTexture(auiWidth, auiHeight, 1, D3DUSAGE_RENDERTARGET, D3DFMT_A32B32G32R32F, D3DPOOL_DEFAULT, &pGroundTexture, nullptr))
			|| FAILED(pGroundTexture->GetSurfaceLevel(0, &pGroundSurface))) {
			_MESSAGE("Failed to create the ground buffer (%u x %u A32B32G32R32F), terrain overlay disabled", auiWidth, auiHeight);
			SafeRelease(pGroundSurface);
			SafeRelease(pGroundTexture);
			bReady = false;
			return false;
		}

		uiGroundWidth	= auiWidth;
		uiGroundHeight	= auiHeight;
		_MESSAGE("Ground buffer created (%u x %u)", auiWidth, auiHeight);
		return true;
	}

	void Initialize() {
		pVertexShader	= Dependencies::CreateVertexShader("IntersectionBlend\\GroundBuffer.vso");
		pPixelShader	= Dependencies::CreatePixelShader("IntersectionBlend\\GroundBuffer.pso");
		if (!pVertexShader || !pPixelShader) {
			_MESSAGE("Failed to load ground buffer shaders (VS %p, PS %p), terrain overlay disabled", pVertexShader, pPixelShader);
			return;
		}

		pVertexShader->IncRefCount();
		pPixelShader->IncRefCount();

		const D3DVERTEXELEMENT9 kElements[] = {
			{ 0, 0,  D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
			{ 0, 12, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
			D3DDECL_END()
		};
		NiDX9Renderer* pRenderer = NiDX9Renderer::GetSingleton();
		if (FAILED(pRenderer->GetD3DDevice()->CreateVertexDeclaration(kElements, &pDeclaration))) {
			_MESSAGE("Failed to create the ground buffer vertex declaration, terrain overlay disabled");
			return;
		}

		pRenderer->AddResetNotificationFunc(OnDeviceReset, nullptr);
		bReady = true;
	}

	// Columns of the world rotation: Gamebryo cameras look along X, with Y up and Z right (see NiRenderer::SetCameraData, 0x4E9BB0).
	static NiPoint3 GetColumn(const NiMatrix3& arMatrix, uint32_t auiColumn) {
		return NiPoint3(arMatrix.m_pEntry[0][auiColumn], arMatrix.m_pEntry[1][auiColumn], arMatrix.m_pEntry[2][auiColumn]);
	}

	static NiPoint3 Cross(const NiPoint3& a, const NiPoint3& b) {
		return NiPoint3(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x);
	}

	// Draws the landscape passes of apTarget that are not in the height field yet, or all of them after a recenter.
	static void UpdateField(IDirect3DDevice9* apDevice, const NiCamera* apCamera, const BSRenderedTexture* apTarget) {
		const NiTransform& kCameraWorld = apCamera->m_kWorld;
		const NiPoint3& kEye = kCameraWorld.m_Translate;

		const float fDX = kEye.x - kFieldCenter.x;
		const float fDY = kEye.y - kFieldCenter.y;
		const bool bFull = !bFieldValid || fDX * fDX + fDY * fDY > FIELD_RECENTER_DISTANCE * FIELD_RECENTER_DISTANCE;

		if (bFull) {
			// Snap the centre to the texel grid, so heights don't shift between recenters.
			const float fTexel = Settings::fMaxDistance * 2.0f / FIELD_SIZE;
			kFieldCenter = NiPoint3(std::floor(kEye.x / fTexel) * fTexel, std::floor(kEye.y / fTexel) * fTexel, kEye.z + FIELD_DEPTH * 0.5f);
			kFieldGeometry.clear();
		}
		else if (!TerrainPasses::Any(apTarget, [](const NiGeometry* apGeometry) { return !IsInField(apGeometry); })) {
			return;
		}

		NiDX9Renderer* pRenderer = NiDX9Renderer::GetSingleton();
		NiDX9RenderState* pRenderState = pRenderer->m_pkRenderState;

		IDirect3DSurface9* pSavedTarget = nullptr;
		IDirect3DSurface9* pSavedDepth = nullptr;
		D3DVIEWPORT9 kSavedViewport;
		apDevice->GetRenderTarget(0, &pSavedTarget);
		apDevice->GetDepthStencilSurface(&pSavedDepth);
		apDevice->GetViewport(&kSavedViewport);

		// Orthographic camera straight down, with the same handedness as the scene camera.
		const NiPoint3 kSceneDirection = GetColumn(kCameraWorld.m_Rotate, 0);
		const NiPoint3 kSceneUp = GetColumn(kCameraWorld.m_Rotate, 1);
		const NiPoint3 kSceneRight = GetColumn(kCameraWorld.m_Rotate, 2);
		const NiPoint3 kSceneCross = Cross(kSceneDirection, kSceneUp);
		const float fHandedness = kSceneCross.x * kSceneRight.x + kSceneCross.y * kSceneRight.y + kSceneCross.z * kSceneRight.z < 0.0f ? -1.0f : 1.0f;

		const NiPoint3 kDirection(0.0f, 0.0f, -1.0f);
		const NiPoint3 kUp(0.0f, 1.0f, 0.0f);
		const NiPoint3 kCross = Cross(kDirection, kUp);
		const NiPoint3 kRight(kCross.x * fHandedness, kCross.y * fHandedness, kCross.z * fHandedness);
		const float fHalfExtent = Settings::fMaxDistance;
		const NiFrustum kFrustum(-fHalfExtent, fHalfExtent, fHalfExtent, -fHalfExtent, 1.0f, FIELD_DEPTH, true);
		const NiRect<float> kPort(0.0f, 1.0f, 1.0f, 0.0f);

		ThisCall(SET_CAMERA_DATA, pRenderer, &kFieldCenter, &kDirection, &kUp, &kRight, &kFrustum, &kPort);
		D3DXMatrixMultiply(&kFieldViewProj, pRenderer->GetD3DView(), pRenderer->GetD3DProj());

		static uint32_t uiLoggedCameras = 0;
		if (uiLoggedCameras < 5) {
			uiLoggedCameras++;
			D3DXMATRIX kInverseView;
			D3DXMatrixInverse(&kInverseView, nullptr, pRenderer->GetD3DView());
			_MESSAGE("Height field camera: requested at %.1f, %.1f, %.1f, view matrix places it at %.1f, %.1f, %.1f (camera eye %.1f, %.1f, %.1f)",
				kFieldCenter.x, kFieldCenter.y, kFieldCenter.z, kInverseView._41, kInverseView._42, kInverseView._43, kEye.x, kEye.y, kEye.z);
		}

		apDevice->SetRenderTarget(0, pFieldColor);
		apDevice->SetDepthStencilSurface(pFieldDepth);
		if (bFull)
			apDevice->Clear(0, nullptr, D3DCLEAR_ZBUFFER, 0, 0.0f, 0); // Reversed depth: 0 is the far plane.

		// Depth Resolve's RESZ resolve binds the depth texture to stage 0 without going through the game's texture cache.
		for (uint32_t i = 0; i < TERRAIN_SAMPLER_COUNT; i++)
			pRenderState->SetTexture(i, nullptr);

		uint32_t uiDrawn = 0;
		{
			ForcedRenderStates kStates(pRenderState);
			kStates.Force(D3DRS_ZENABLE, D3DZB_TRUE);
			kStates.Force(D3DRS_ZWRITEENABLE, TRUE);
			kStates.Force(D3DRS_COLORWRITEENABLE, 0);
			kStates.Force(D3DRS_ALPHABLENDENABLE, FALSE);
			kStates.Force(D3DRS_ALPHATESTENABLE, FALSE);
			kStates.Force(D3DRS_ADAPTIVETESS_Y, 0); // Alpha to coverage, NVIDIA only.
			kStates.Force(D3DRS_STENCILENABLE, FALSE);
			kStates.Force(D3DRS_SCISSORTESTENABLE, FALSE);
			kStates.Force(D3DRS_CULLMODE, D3DCULL_NONE);

			// Not through NiDX9RenderState: LOD Flicker Fix remaps every D3DRS_ZFUNC set there (see ForcedRenderStates).
			// Keep the highest terrain with the reversed depth, then put back what the game's cache holds.
			apDevice->SetRenderState(D3DRS_ZFUNC, D3DCMP_GREATEREQUAL);
			uiDrawn = TerrainPasses::Replay(apTarget, AddToField);
			apDevice->SetRenderState(D3DRS_ZFUNC, pRenderState->m_akRenderStateSettings[D3DRS_ZFUNC].m_uiCurrValue);
		}

		apDevice->SetRenderTarget(0, pSavedTarget);
		apDevice->SetDepthStencilSurface(pSavedDepth);
		pRenderer->SetCameraData(apCamera);
		apDevice->SetViewport(&kSavedViewport);
		SafeRelease(pSavedTarget);
		SafeRelease(pSavedDepth);

		bFieldValid = true;

		static uint32_t uiLoggedRedraws = 0;
		if (bFull && uiLoggedRedraws < 2) {
			uiLoggedRedraws++;
			_MESSAGE("Height field redraw, landscape passes:");
			TerrainPasses::LogRecorded(apTarget);
		}

		static uint32_t uiLoggedUpdates = 0;
		if (uiLoggedUpdates < 5) {
			_MESSAGE("Height field %s: %u landscape passes drawn, centre %.0f, %.0f", bFull ? "redrawn" : "extended", uiDrawn, kFieldCenter.x, kFieldCenter.y);
			uiLoggedUpdates++;
		}
	}

	static void DrawGround(IDirect3DDevice9* apDevice, const NiCamera* apCamera, const D3DXMATRIX& arView, const D3DXMATRIX& arProjection, IDirect3DBaseTexture9* apDepthTexture) {
		IDirect3DSurface9* pSavedTarget = nullptr;
		IDirect3DSurface9* pSavedDepth = nullptr;
		apDevice->GetRenderTarget(0, &pSavedTarget);
		apDevice->GetDepthStencilSurface(&pSavedDepth);
		pStateBlock->Capture();

		const NiFrustum& kFrustum = apCamera->m_kViewFrustum;
		const NiPoint3& kEye = apCamera->m_kWorld.m_Translate;

		// Eye-relative world positions to height field clip space and back. The renderer works relative to the camera:
		// its view matrices carry no translation, and SetCameraData's location is subtracted from every world transform
		// instead. So the height field's matrix takes positions relative to the field camera.
		D3DXMATRIX kEyeTranslation, kWorldToField, kFieldToWorld;
		D3DXMatrixTranslation(&kEyeTranslation, kEye.x - kFieldCenter.x, kEye.y - kFieldCenter.y, kEye.z - kFieldCenter.z);
		D3DXMatrixMultiply(&kWorldToField, &kEyeTranslation, &kFieldViewProj);
		D3DXMatrixInverse(&kFieldToWorld, nullptr, &kWorldToField);

		const float fDepthParams[4]			= { kFrustum.m_fNear, kFrustum.m_fFar - kFrustum.m_fNear, kFrustum.m_fNear * kFrustum.m_fFar, 0.0f };
		const float fProjectionParams[4]	= { arProjection._11, arProjection._22, arProjection._31, arProjection._32 };
		const float fViewToWorld[3][4] = {
			{ arView._11, arView._21, arView._31, 0.0f },
			{ arView._12, arView._22, arView._32, 0.0f },
			{ arView._13, arView._23, arView._33, 0.0f },
		};
		const float fFieldParams[4]			= { 1.0f / FIELD_SIZE, 1.0f / FIELD_SIZE, Settings::fMaxDistance, 0.0f };
		const float fTexelSize[4]			= { 1.0f / uiGroundWidth, 1.0f / uiGroundHeight, 0.0f, 0.0f };

		apDevice->SetPixelShaderConstantF(0, fDepthParams, 1);
		apDevice->SetPixelShaderConstantF(1, fProjectionParams, 1);
		apDevice->SetPixelShaderConstantF(2, fViewToWorld[0], 3);
		apDevice->SetPixelShaderConstantF(5, kWorldToField, 4);
		apDevice->SetPixelShaderConstantF(9, kFieldToWorld, 4);
		apDevice->SetPixelShaderConstantF(13, fFieldParams, 1);
		apDevice->SetPixelShaderConstantF(14, fTexelSize, 1);
		apDevice->SetVertexShaderConstantF(0, fTexelSize, 1);

		apDevice->SetRenderTarget(0, pGroundSurface);
		apDevice->SetDepthStencilSurface(nullptr);

		apDevice->SetRenderState(D3DRS_ZENABLE, D3DZB_FALSE);
		apDevice->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
		apDevice->SetRenderState(D3DRS_STENCILENABLE, FALSE);
		apDevice->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
		apDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
		apDevice->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
		apDevice->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
		apDevice->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);
		apDevice->SetRenderState(D3DRS_SRGBWRITEENABLE, FALSE);
		apDevice->SetRenderState(D3DRS_COLORWRITEENABLE, D3DCOLORWRITEENABLE_RED | D3DCOLORWRITEENABLE_GREEN | D3DCOLORWRITEENABLE_BLUE | D3DCOLORWRITEENABLE_ALPHA);

		IDirect3DBaseTexture9* const pTextures[] = { apDepthTexture, pFieldTexture };
		for (uint32_t i = 0; i < ARRAYSIZE(pTextures); i++) {
			apDevice->SetTexture(i, pTextures[i]);
			apDevice->SetSamplerState(i, D3DSAMP_MINFILTER, D3DTEXF_POINT);
			apDevice->SetSamplerState(i, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
			apDevice->SetSamplerState(i, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
			apDevice->SetSamplerState(i, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
			apDevice->SetSamplerState(i, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
			apDevice->SetSamplerState(i, D3DSAMP_SRGBTEXTURE, FALSE);
		}

		apDevice->SetVertexDeclaration(pDeclaration);
		apDevice->SetVertexShader(pVertexShader->GetShaderHandle());
		apDevice->SetPixelShader(pPixelShader->GetShaderHandle());

		// Diagnostics for the first frames: texture bindings, and where each matrix puts the eye and the field centre.
		static uint32_t uiLoggedFrames = 0;
		if (uiLoggedFrames < 3) {
			uiLoggedFrames++;

			IDirect3DBaseTexture9* pBound[2] = {};
			apDevice->GetTexture(0, &pBound[0]);
			apDevice->GetTexture(1, &pBound[1]);
			_MESSAGE("Ground pass: scene depth %p, height field %p, bound at s0 %p, s1 %p", apDepthTexture, pFieldTexture, pBound[0], pBound[1]);
			SafeRelease(pBound[0]);
			SafeRelease(pBound[1]);

			D3DXMATRIX kInverseView;
			D3DXMatrixInverse(&kInverseView, nullptr, &arView);
			const D3DXVECTOR4 kFieldOrigin(0.0f, 0.0f, 0.0f, 1.0f);
			D3DXVECTOR4 kFieldCentreRelative;
			D3DXVec4Transform(&kFieldCentreRelative, &kFieldOrigin, &kFieldToWorld);
			_MESSAGE("Ground pass: camera eye %.1f, %.1f, %.1f, view matrix eye %.1f, %.1f, %.1f, field centre %.1f, %.1f, %.1f, field matrix centre relative to the eye %.1f, %.1f, %.1f",
				kEye.x, kEye.y, kEye.z, kInverseView._41, kInverseView._42, kInverseView._43, kFieldCenter.x, kFieldCenter.y, kFieldCenter.z,
				kFieldCentreRelative.x / kFieldCentreRelative.w, kFieldCentreRelative.y / kFieldCentreRelative.w, kFieldCentreRelative.z / kFieldCentreRelative.w);
		}

		const FullscreenVertex kQuad[4] = {
			{ -1.0f,  1.0f, 0.0f, 0.0f, 0.0f },
			{  1.0f,  1.0f, 0.0f, 1.0f, 0.0f },
			{ -1.0f, -1.0f, 0.0f, 0.0f, 1.0f },
			{  1.0f, -1.0f, 0.0f, 1.0f, 1.0f },
		};
		apDevice->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, kQuad, sizeof(FullscreenVertex));

		// Render targets are not part of state blocks. Setting them resets the viewport, which the state block restores.
		apDevice->SetRenderTarget(0, pSavedTarget);
		apDevice->SetDepthStencilSurface(pSavedDepth);
		pStateBlock->Apply();
		SafeRelease(pSavedTarget);
		SafeRelease(pSavedDepth);
	}

	IDirect3DTexture9* Render(IDirect3DDevice9* apDevice, const NiCamera* apCamera, const BSRenderedTexture* apTarget, IDirect3DBaseTexture9* apDepthTexture) {
		if (!bReady || !apCamera || !apDepthTexture)
			return nullptr;

		IDirect3DSurface9* pTarget = nullptr;
		if (FAILED(apDevice->GetRenderTarget(0, &pTarget)))
			return nullptr;
		D3DSURFACE_DESC kDesc;
		pTarget->GetDesc(&kDesc);
		pTarget->Release();

		if (!CreateFieldResources(apDevice) || !CreateGroundResources(apDevice, kDesc.Width, kDesc.Height))
			return nullptr;

		// The scene camera's matrices, before the height field camera replaces them.
		NiDX9Renderer* pRenderer = NiDX9Renderer::GetSingleton();
		const D3DXMATRIX kView = *pRenderer->GetD3DView();
		const D3DXMATRIX kProjection = *pRenderer->GetD3DProj();

		UpdateField(apDevice, apCamera, apTarget);
		DrawGround(apDevice, apCamera, kView, kProjection, apDepthTexture);
		return pGroundTexture;
	}
}
