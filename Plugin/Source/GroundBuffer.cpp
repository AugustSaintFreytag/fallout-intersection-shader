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

#include <cmath>
#include <unordered_map>

// LOD Flicker Fix patches `m_kD3DProj._33` and `_43` at +0x9E8 and +0x9F8 (GAME - 0xE6CBB7).
ASSERT_OFFSET(NiDX9Renderer, m_kD3DProj, 0x9C0);

// Builds what the terrain overlay knows about the ground under each visible surface.
//
// Two steps run right before the overlay:
// 1. Height field: an orthographic camera looking straight down draws the recorded landscape passes again, into a
//    depth texture around the camera. Terrain does not move. The field is only redrawn in full when the camera moves
//    away from its centre. In between, only landscape blocks not in it yet are drawn. The terrain shaders write blend
//    weights and vertex colour into the attribute fields in the same draw. Both are per vertex, 128 units apart.
//    The fields hold them without loss at 8 units per texel.
// 2. Ground buffer: a full-screen pass rebuilds every visible surface point from the scene depth and looks up the
//    terrain under it. It writes linear depth, height above the terrain and the terrain normal per pixel. In ground
//    mode it writes the ground point that shades the surface instead, with the overlay opacity and the attributes there.
//
// The overlay reads the two ground buffer targets from s14 and s15.
//
// Reference at "Shaders/Ground/GroundBuffer.hlsl", "PS_OUTPUT".
namespace GroundBuffer {

	// Height field: `FIELD_SIZE`² texels over twice `[Blend] fMaxDistance` around the camera.
	// 8 units per texel by default. Terrain vertices are 128 units apart.
	// The field camera sits `FIELD_DEPTH` / 2 above the scene camera and sees `FIELD_DEPTH` down.
	static constexpr uint32_t	FIELD_SIZE				= 1024;
	static constexpr float		FIELD_DEPTH				= 65536.0f;
	static constexpr float		FIELD_RECENTER_DISTANCE	= 512.0f;
	static constexpr uint32_t	TERRAIN_SAMPLER_COUNT	= 14;	// `BaseMap[7]` and `NormalMap[7]`

	// Must match "Shaders/Includes/TerrainOverlay.hlsli". In the height field replay, the terrain shaders write blend
	// weights and vertex colour into the attribute fields instead of shading.
	static constexpr float		FIELD_MAGIC				= 7907.0f;
	static constexpr uint32_t	FIELD_CONSTANT_REGISTER	= 89;	// `IBOverlayFlags`
	static constexpr uint32_t	BLOCK_CONSTANT_REGISTER	= 92;	// `IBBlock`
	static constexpr float		LANDSCAPE_BLOCK_SIZE	= 2048.0f;

	// Attribute fields, one per target: blend weights 0-3, blend weights 4-6 with the block code, vertex colour.
	static constexpr uint32_t	FIELD_TARGET_COUNT		= 3;
	static constexpr uint32_t	FIELD_SAMPLER			= 2;	// s2-s4 in the ground pass

	// `NiRenderer::SetCameraData(location, direction, up, right, frustum, port)` (GAME - 0x4E9C90).
	// Checks that a frame is in progress and calls `NiDX9Renderer::Do_SetCameraData` through the vtable.
	static constexpr uint32_t	SET_CAMERA_DATA			= 0x4E9C90;

	struct FullscreenVertex {
		float x, y, z;
		float u, v;
	};

	static NiD3DVertexShader*			pVertexShader		= nullptr;
	static NiD3DPixelShader*			pPixelShader		= nullptr;
	static IDirect3DVertexDeclaration9*	pDeclaration		= nullptr;
	static bool							bReady				= false;

	// Device Resources
	// Released before a device reset and created again on demand.
	static IDirect3DTexture9*			pFieldTexture		= nullptr;	// INTZ
	static IDirect3DSurface9*			pFieldDepth			= nullptr;
	static IDirect3DTexture9*			pAttributeTextures[FIELD_TARGET_COUNT]	= {};	// A8R8G8B8, texel for texel with the height field
	static IDirect3DSurface9*			pAttributeSurfaces[FIELD_TARGET_COUNT]	= {};
	static IDirect3DTexture9*			pGroundTexture		= nullptr;
	static IDirect3DSurface9*			pGroundSurface		= nullptr;
	static IDirect3DTexture9*			pPointTexture		= nullptr;	// Ground points, the second ground pass target.
	static IDirect3DSurface9*			pPointSurface		= nullptr;
	static IDirect3DStateBlock9*		pStateBlock			= nullptr;
	static uint32_t						uiGroundWidth		= 0;
	static uint32_t						uiGroundHeight		= 0;

	// Height Field Contents
	static bool							bFieldValid			= false;
	static NiPoint3						kFieldCenter;
	static D3DXMATRIX					kFieldViewProj;
	// Landscape geometry drawn into the height field, with its bound centre at the time.
	// Geometry is freed when cells unload and its address reused. A block only counts as drawn while it stays in place.
	static std::unordered_map<const NiGeometry*, NiPoint3> kFieldGeometry;

	static bool IsInField(const NiGeometry* apGeometry) {
		const auto it = kFieldGeometry.find(apGeometry);
		const NiBound* pBound = apGeometry->m_pWorldBound;
		return it != kFieldGeometry.end() && pBound
			&& it->second.x == pBound->m_kCenter.x && it->second.y == pBound->m_kCenter.y && it->second.z == pBound->m_kCenter.z;
	}

	// Records the given geometry as drawn into the height field. Returns false if it was already drawn.
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
		for (uint32_t i = 0; i < FIELD_TARGET_COUNT; i++) {
			SafeRelease(pAttributeSurfaces[i]);
			SafeRelease(pAttributeTextures[i]);
		}
		SafeRelease(pGroundSurface);
		SafeRelease(pGroundTexture);
		SafeRelease(pPointSurface);
		SafeRelease(pPointTexture);
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

	// Creates the height field and the attribute fields if missing.
	// Disables the ground buffer and returns false if creation fails.
	static bool CreateFieldResources(IDirect3DDevice9* apDevice) {
		if (pFieldDepth)
			return true;

		bool bCreated = SUCCEEDED(apDevice->CreateTexture(FIELD_SIZE, FIELD_SIZE, 1, D3DUSAGE_DEPTHSTENCIL, static_cast<D3DFORMAT>(MAKEFOURCC('I', 'N', 'T', 'Z')), D3DPOOL_DEFAULT, &pFieldTexture, nullptr))
			&& SUCCEEDED(pFieldTexture->GetSurfaceLevel(0, &pFieldDepth));
		for (uint32_t i = 0; i < FIELD_TARGET_COUNT && bCreated; i++) {
			bCreated = SUCCEEDED(apDevice->CreateTexture(FIELD_SIZE, FIELD_SIZE, 1, D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &pAttributeTextures[i], nullptr))
				&& SUCCEEDED(pAttributeTextures[i]->GetSurfaceLevel(0, &pAttributeSurfaces[i]));
		}
		if (!bCreated) {
			_MESSAGE("Failed to create the height field (%u x %u INTZ and %u A8R8G8B8 attribute fields), terrain overlay disabled", FIELD_SIZE, FIELD_SIZE, FIELD_TARGET_COUNT);
			ReleaseResources();
			bReady = false;
			return false;
		}

		_MESSAGE("Height field created (%u x %u over %.0f units)", FIELD_SIZE, FIELD_SIZE, Settings::fMaxDistance * 2.0f);
		return true;
	}

	// Creates the ground buffer targets at the given size if missing or sized differently.
	// Disables the ground buffer and returns false if creation fails.
	static bool CreateGroundResources(IDirect3DDevice9* apDevice, uint32_t auiWidth, uint32_t auiHeight) {
		if (pGroundSurface && uiGroundWidth == auiWidth && uiGroundHeight == auiHeight)
			return true;

		SafeRelease(pGroundSurface);
		SafeRelease(pGroundTexture);
		SafeRelease(pPointSurface);
		SafeRelease(pPointTexture);

		if (!pStateBlock && FAILED(apDevice->CreateStateBlock(D3DSBT_ALL, &pStateBlock))) {
			_MESSAGE("Failed to create the ground buffer state block, terrain overlay disabled");
			bReady = false;
			return false;
		}

		if (FAILED(apDevice->CreateTexture(auiWidth, auiHeight, 1, D3DUSAGE_RENDERTARGET, D3DFMT_A32B32G32R32F, D3DPOOL_DEFAULT, &pGroundTexture, nullptr))
			|| FAILED(pGroundTexture->GetSurfaceLevel(0, &pGroundSurface))
			|| FAILED(apDevice->CreateTexture(auiWidth, auiHeight, 1, D3DUSAGE_RENDERTARGET, D3DFMT_A32B32G32R32F, D3DPOOL_DEFAULT, &pPointTexture, nullptr))
			|| FAILED(pPointTexture->GetSurfaceLevel(0, &pPointSurface))) {
			_MESSAGE("Failed to create the ground buffers (2 x %u x %u A32B32G32R32F), terrain overlay disabled", auiWidth, auiHeight);
			SafeRelease(pGroundSurface);
			SafeRelease(pGroundTexture);
			SafeRelease(pPointSurface);
			SafeRelease(pPointTexture);
			bReady = false;
			return false;
		}

		uiGroundWidth	= auiWidth;
		uiGroundHeight	= auiHeight;
		_MESSAGE("Ground buffer created (%u x %u)", auiWidth, auiHeight);
		return true;
	}

	// Loads the ground buffer shaders and registers for device resets.
	// Call once the D3D device exists, at NVSE DeferredInit.
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

	// Returns a column of the given rotation. Gamebryo cameras look along column 0, with column 1 up and column 2 right.
	//
	// Reference at `NiRenderer::SetCameraData` (GAME - 0x4E9BB0).
	static NiPoint3 GetColumn(const NiMatrix3& arMatrix, uint32_t auiColumn) {
		return NiPoint3(arMatrix.m_pEntry[0][auiColumn], arMatrix.m_pEntry[1][auiColumn], arMatrix.m_pEntry[2][auiColumn]);
	}

	static NiPoint3 Cross(const NiPoint3& a, const NiPoint3& b) {
		return NiPoint3(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x);
	}

	// Draws the landscape passes of the given target that are not in the height field yet.
	// Recenters and redraws the whole field if the camera moved too far from its centre.
	static void UpdateField(IDirect3DDevice9* apDevice, const NiCamera* apCamera, const BSRenderedTexture* apTarget) {
		const NiTransform& kCameraWorld = apCamera->m_kWorld;
		const NiPoint3& kEye = kCameraWorld.m_Translate;

		const float fDX = kEye.x - kFieldCenter.x;
		const float fDY = kEye.y - kFieldCenter.y;
		const bool bFull = !bFieldValid || fDX * fDX + fDY * fDY > FIELD_RECENTER_DISTANCE * FIELD_RECENTER_DISTANCE;

		if (bFull) {
			// Snap the centre to the texel grid. Heights then stay in place between recenters.
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

		for (uint32_t i = 0; i < FIELD_TARGET_COUNT; i++)
			apDevice->SetRenderTarget(i, pAttributeSurfaces[i]);
		apDevice->SetDepthStencilSurface(pFieldDepth);
		if (bFull)
			apDevice->Clear(0, nullptr, D3DCLEAR_ZBUFFER, 0, 0.0f, 0); // Reversed depth: 0 is the far plane.

		// Set on the device directly. No game shader uses these registers.
		const float fFieldFlags[4] = { FIELD_MAGIC, 0.0f, 0.0f, 0.0f };
		apDevice->SetPixelShaderConstantF(FIELD_CONSTANT_REGISTER, fFieldFlags, 1);

		// Depth Resolve's RESZ resolve binds the depth texture to stage 0 behind the game's texture cache.
		for (uint32_t i = 0; i < TERRAIN_SAMPLER_COUNT; i++)
			pRenderState->SetTexture(i, nullptr);

		uint32_t uiDrawn = 0;
		{
			ForcedRenderStates kStates(pRenderState);
			kStates.Force(D3DRS_ZENABLE, D3DZB_TRUE);
			kStates.Force(D3DRS_ZWRITEENABLE, TRUE);
			kStates.Force(D3DRS_COLORWRITEENABLE, D3DCOLORWRITEENABLE_RED | D3DCOLORWRITEENABLE_GREEN | D3DCOLORWRITEENABLE_BLUE | D3DCOLORWRITEENABLE_ALPHA);
			kStates.Force(D3DRS_COLORWRITEENABLE1, D3DCOLORWRITEENABLE_RED | D3DCOLORWRITEENABLE_GREEN | D3DCOLORWRITEENABLE_BLUE | D3DCOLORWRITEENABLE_ALPHA);
			kStates.Force(D3DRS_COLORWRITEENABLE2, D3DCOLORWRITEENABLE_RED | D3DCOLORWRITEENABLE_GREEN | D3DCOLORWRITEENABLE_BLUE | D3DCOLORWRITEENABLE_ALPHA);
			kStates.Force(D3DRS_SRGBWRITEENABLE, FALSE);
			kStates.Force(D3DRS_ALPHABLENDENABLE, FALSE);
			kStates.Force(D3DRS_ALPHATESTENABLE, FALSE);
			kStates.Force(D3DRS_ADAPTIVETESS_Y, 0); // Alpha to coverage, NVIDIA only.
			kStates.Force(D3DRS_STENCILENABLE, FALSE);
			kStates.Force(D3DRS_SCISSORTESTENABLE, FALSE);
			kStates.Force(D3DRS_CULLMODE, D3DCULL_NONE);

			// Set on the device directly. LOD Flicker Fix remaps every `D3DRS_ZFUNC` set through `NiDX9RenderState`.
			// `D3DCMP_GREATEREQUAL` keeps the highest terrain with reversed depth. The cached value is restored after the replay.
			apDevice->SetRenderState(D3DRS_ZFUNC, D3DCMP_GREATEREQUAL);
			// The terrain shaders store the block code with the attributes.
			// Reference at "Shaders/Includes/Packing.hlsli", "GetBlockCode".
			uiDrawn = TerrainPasses::Replay(apTarget, AddToField, [apDevice](const NiGeometry* apGeometry) {
				const NiBound* pBound = apGeometry->m_pWorldBound;
				const float fBlock[4] = {
					pBound ? std::floor(pBound->m_kCenter.x / LANDSCAPE_BLOCK_SIZE) : 0.0f,
					pBound ? std::floor(pBound->m_kCenter.y / LANDSCAPE_BLOCK_SIZE) : 0.0f,
					0.0f, 0.0f,
				};
				apDevice->SetPixelShaderConstantF(BLOCK_CONSTANT_REGISTER, fBlock, 1);
			});
			apDevice->SetRenderState(D3DRS_ZFUNC, pRenderState->m_akRenderStateSettings[D3DRS_ZFUNC].m_uiCurrValue);
		}

		const float fZero[4] = {};
		apDevice->SetPixelShaderConstantF(FIELD_CONSTANT_REGISTER, fZero, 1);
		apDevice->SetPixelShaderConstantF(BLOCK_CONSTANT_REGISTER, fZero, 1);

		for (uint32_t i = 1; i < FIELD_TARGET_COUNT; i++)
			apDevice->SetRenderTarget(i, nullptr);
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

	// Renders the ground buffer from the scene depth and the height field.
	static void DrawGround(IDirect3DDevice9* apDevice, const NiCamera* apCamera, const D3DXMATRIX& arView, const D3DXMATRIX& arProjection, IDirect3DBaseTexture9* apDepthTexture, bool abGroundMode) {
		IDirect3DSurface9* pSavedTarget = nullptr;
		IDirect3DSurface9* pSavedDepth = nullptr;
		apDevice->GetRenderTarget(0, &pSavedTarget);
		apDevice->GetDepthStencilSurface(&pSavedDepth);
		pStateBlock->Capture();

		const NiFrustum& kFrustum = apCamera->m_kViewFrustum;
		const NiPoint3& kEye = apCamera->m_kWorld.m_Translate;

		// Eye-relative world positions to height field clip space and back.
		// The renderer works relative to the camera. Its view matrices carry no translation. `SetCameraData` subtracts
		// its location from every world transform instead. The height field matrix takes positions relative to the field camera.
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
		const float fGroundParams[4]		= { abGroundMode ? 1.0f : 0.0f, Settings::fBlendRange, Settings::bUnrolledMapping ? 1.0f : 0.0f, 0.0f };
		const float fEyePosition[4]			= { kEye.x, kEye.y, kEye.z, 0.0f };

		apDevice->SetPixelShaderConstantF(0, fDepthParams, 1);
		apDevice->SetPixelShaderConstantF(1, fProjectionParams, 1);
		apDevice->SetPixelShaderConstantF(2, fViewToWorld[0], 3);
		apDevice->SetPixelShaderConstantF(5, kWorldToField, 4);
		apDevice->SetPixelShaderConstantF(9, kFieldToWorld, 4);
		apDevice->SetPixelShaderConstantF(13, fFieldParams, 1);
		apDevice->SetPixelShaderConstantF(14, fTexelSize, 1);
		apDevice->SetPixelShaderConstantF(15, fGroundParams, 1);
		apDevice->SetPixelShaderConstantF(16, fEyePosition, 1);
		apDevice->SetVertexShaderConstantF(0, fTexelSize, 1);

		apDevice->SetRenderTarget(0, pGroundSurface);
		apDevice->SetRenderTarget(1, pPointSurface);
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
		apDevice->SetRenderState(D3DRS_COLORWRITEENABLE1, D3DCOLORWRITEENABLE_RED | D3DCOLORWRITEENABLE_GREEN | D3DCOLORWRITEENABLE_BLUE | D3DCOLORWRITEENABLE_ALPHA);

		IDirect3DBaseTexture9* const pTextures[] = { apDepthTexture, pFieldTexture, pAttributeTextures[0], pAttributeTextures[1], pAttributeTextures[2] };
		static_assert(ARRAYSIZE(pTextures) == FIELD_SAMPLER + FIELD_TARGET_COUNT, "attribute fields follow the height field");
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

		// Log texture bindings and where each matrix puts the eye and field centre. First frames only.
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

		// Render targets are not part of state blocks. Setting them resets the viewport. The state block restores it.
		apDevice->SetRenderTarget(1, nullptr);
		apDevice->SetRenderTarget(0, pSavedTarget);
		apDevice->SetDepthStencilSurface(pSavedDepth);
		pStateBlock->Apply();
		SafeRelease(pSavedTarget);
		SafeRelease(pSavedDepth);
	}

	// Updates the height field from the passes recorded into the given target and renders the ground buffer.
	// Uses the ground mode layout if requested. Runs while the target is bound.
	// Returns the first ground buffer target, or null if it is unavailable.
	IDirect3DTexture9* Render(IDirect3DDevice9* apDevice, const NiCamera* apCamera, const BSRenderedTexture* apTarget, IDirect3DBaseTexture9* apDepthTexture, bool abGroundMode) {
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

		// Copy the scene camera's matrices before the height field camera replaces them.
		NiDX9Renderer* pRenderer = NiDX9Renderer::GetSingleton();
		const D3DXMATRIX kView = *pRenderer->GetD3DView();
		const D3DXMATRIX kProjection = *pRenderer->GetD3DProj();

		UpdateField(apDevice, apCamera, apTarget);
		DrawGround(apDevice, apCamera, kView, kProjection, apDepthTexture, abGroundMode);
		return pGroundTexture;
	}

	// Returns the second ground buffer target, written by the last `Render`.
	IDirect3DTexture9* GetPointTexture() {
		return pPointTexture;
	}

}
