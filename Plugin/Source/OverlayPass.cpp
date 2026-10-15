#include "OverlayPass.hpp"

#include "Dependencies.hpp"
#include "DrawRedirect.hpp"
#include "GroundBuffer.hpp"
#include "Profiler.hpp"
#include "RenderStates.hpp"
#include "Settings.hpp"
#include "StencilTags.hpp"
#include "TerrainPasses.hpp"

#include "Bethesda/BSShaderAccumulator.hpp"
#include "Gamebryo/NiCamera.hpp"
#include "Gamebryo/NiD3DPixelShader.hpp"
#include "Gamebryo/NiD3DVertexShader.hpp"
#include "Gamebryo/NiDX9Renderer.hpp"
#include "Gamebryo/NiGeometry.hpp"

#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>

// Draws the terrain overlay between Depth Resolve's depth copy and the post-depth effects such as AO.
//
// The landscape passes recorded in the opaque stage are drawn again, without depth test and with alpha blending.
// The terrain pixel shaders switch into overlay mode for this replay. They read the ground buffer and output an
// opacity that falls off with the height of the visible surface above the terrain.
//
// Reference at "Shaders/Includes/TerrainOverlay.hlsli".
namespace OverlayPass {

	// Must match "Shaders/Includes/TerrainOverlay.hlsli".
	static constexpr float		OVERLAY_MAGIC				= 7919.0f;
	static constexpr uint32_t	OVERLAY_CONSTANT_REGISTER	= 89;	// c89-c96
	static constexpr uint32_t	OVERLAY_CONSTANT_COUNT		= 8;
	static constexpr uint32_t	BLOCK_CONSTANT_REGISTER		= 92;	// c92-c93, per pass in ground mode
	static constexpr uint32_t	GROUND_BUFFER_SAMPLER		= 14;	// Ground buffer, then the ground points at s15.
	static constexpr uint32_t	GROUND_SAMPLER_COUNT		= 2;
	static constexpr float		LANDSCAPE_BLOCK_SIZE		= 2048.0f;
	static constexpr float		GROUND_MODE					= 3.0f;

	// Reach of the lip past the surface it shades, in multiples of `fBlendRange`. The unrolled mapping moves the
	// ground point up to ~2.6 times the height past the foot. The height of a shaded surface stays within the range.
	static constexpr float		LIP_REACH					= 4.0f;
	static constexpr uint32_t	TERRAIN_SAMPLER_COUNT		= 14;	// `BaseMap[7]` and `NormalMap[7]`
	static constexpr const char* OVERLAY_SHADER_MARKER		= "IBOverlayFlags";

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
	static uint32_t						uiFrameCallbacks		= 0;

	// Checks that every terrain pixel shader in `Data\Shaders\Loose\` is an Intersection Blend build.
	// With the original shaders, the replay would paint plain terrain over everything.
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

	// Checks the terrain shaders and initializes stencil tagging, the ground buffer, the draw redirect and the debug views.
	// Call once the D3D device exists, at NVSE DeferredInit.
	void Initialize() {
		bOverlayShadersReady = CheckOverlayShaders();
		if (bOverlayShadersReady)
			_MESSAGE("Terrain overlay shaders found");
		StencilTags::SetEnabled(Settings::bStencilTags && (bOverlayShadersReady || Settings::eDebugMode == Settings::DEBUG_STENCIL_TAGS));
		GroundBuffer::Initialize();
		DrawRedirect::Initialize();
		Profiler::Initialize();

		pDebugVS = Dependencies::CreateVertexShader("IntersectionBlend\\DebugDepth.vso");
		pDebugPS = Dependencies::CreatePixelShader("IntersectionBlend\\DebugDepth.pso");
		if (!pDebugVS || !pDebugPS) {
			_MESSAGE("Failed to load debug shaders (VS %p, PS %p), debug views disabled", pDebugVS, pDebugPS);
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
			_MESSAGE("Failed to create the fullscreen vertex declaration, debug views disabled");
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

	static LARGE_INTEGER GetCounter() {
		LARGE_INTEGER kCounter;
		QueryPerformanceCounter(&kCounter);
		return kCounter;
	}

	static double GetMillisecondsSince(const LARGE_INTEGER& arStart) {
		LARGE_INTEGER kFrequency;
		QueryPerformanceFrequency(&kFrequency);
		return static_cast<double>(GetCounter().QuadPart - arStart.QuadPart) * 1000.0 / static_cast<double>(kFrequency.QuadPart);
	}

	// Returns whether the overlay runs in ground mode this frame.
	// Ground mode needs the draw redirect. The ground height debug view needs the other ground buffer layout.
	static bool IsGroundMode() {
		return Settings::bGroundShading && DrawRedirect::IsReady() && Settings::eDebugMode != Settings::DEBUG_GROUND_HEIGHT;
	}

	// Returns whether the debug mode shows the ground mode attributes in place of the overlay.
	static bool IsAttributeView() {
		return Settings::eDebugMode == Settings::DEBUG_VERTEX_COLOR || Settings::eDebugMode == Settings::DEBUG_BLEND_WEIGHTS
			|| Settings::eDebugMode == Settings::DEBUG_RAW_VERTEX_COLOR;
	}

	// Returns whether any part of a landscape block's bound lies within the given view depth of the camera.
	static bool IsBlockInRange(const NiGeometry* apGeometry, const NiPoint3& arEye, const NiPoint3& arViewDirection, float afMaxDepth) {
		const NiBound* pBound = apGeometry->m_pWorldBound;
		if (!pBound)
			return true;

		const float fDepth = (pBound->m_kCenter.x - arEye.x) * arViewDirection.x + (pBound->m_kCenter.y - arEye.y) * arViewDirection.y
			+ (pBound->m_kCenter.z - arEye.z) * arViewDirection.z;
		return fDepth - pBound->m_fRadius <= afMaxDepth;
	}

	// Returns the lipping mode for `IBOverlayFlags.w`.
	static float GetLippingMode(bool abGroundMode) {
		if (abGroundMode)
			return GROUND_MODE;
		if (!Settings::bLipping)
			return 0.0f;
		return Settings::bUnrolledMapping ? 2.0f : 1.0f;
	}

	// Replays the landscape passes over the tagged pixels as the terrain overlay. Returns the number of passes drawn.
	// Takes the scene camera's view projection from before the ground buffer pass changed the renderer's matrices.
	static uint32_t RenderTerrainOverlay(IDirect3DDevice9* apDevice, const NiCamera* apCamera, const BSRenderedTexture* apTarget, IDirect3DBaseTexture9* apGroundTexture, IDirect3DBaseTexture9* apPointTexture, const D3DXMATRIX& arViewProjection, bool abGroundMode) {
		NiDX9RenderState* pRenderState = NiDX9Renderer::GetSingleton()->m_pkRenderState;
		const float fLippingMode = GetLippingMode(abGroundMode);

		D3DVIEWPORT9 kViewport;
		apDevice->GetViewport(&kViewport);

		// Set on the device directly. No game shader uses these registers.
		// c90: camera position in world space. The terrain shaders work in the model space of the landscape block.
		// That space is world-aligned. The camera position places their fragments in the world (`IsSameGround`).
		const NiPoint3& kEye = apCamera->m_kWorld.m_Translate;

		// Only blocks that can hold a shaded ground point are replayed. Ground mode shades points near the surface,
		// within `fMaxDistance`. The other modes draw terrain up to `fMaxRayGap` behind the surface. Gamebryo cameras
		// look along their local x axis.
		const NiMatrix3& kRotate = apCamera->m_kWorld.m_Rotate;
		const NiPoint3 kViewDirection(kRotate.m_pEntry[0][0], kRotate.m_pEntry[1][0], kRotate.m_pEntry[2][0]);
		const float fMaxBlockDepth = Settings::fMaxDistance + Settings::fBlendRange * LIP_REACH + (abGroundMode ? 0.0f : Settings::fMaxRayGap);
		const TerrainPasses::PassFilter kInRange = [&kEye, &kViewDirection, fMaxBlockDepth](const NiGeometry* apGeometry) {
			return IsBlockInRange(apGeometry, kEye, kViewDirection, fMaxBlockDepth);
		};
		// c92-c93: block index and eye in block model space, set per pass in ground mode.
		// c94-c96: eye-relative world position to the fog position the terrain shaders build from the clip position:
		// clip x, clip y, w - z. The renderer works relative to the camera. The scene's view projection takes these positions.
		const D3DXMATRIX& rVP = arViewProjection;
		const float fConstants[OVERLAY_CONSTANT_COUNT][4] = {
			{ OVERLAY_MAGIC, static_cast<float>(Settings::eDebugMode), Settings::fMaxRayGap, fLippingMode },
			{ kEye.x, kEye.y, kEye.z, 0.0f },
			{ 1.0f / kViewport.Width, 1.0f / kViewport.Height, Settings::fBlendRange, Settings::fMaxDistance },
			{ 0.0f, 0.0f, 0.0f, 0.0f },
			{ 0.0f, 0.0f, 0.0f, 0.0f },
			{ rVP._11, rVP._21, rVP._31, rVP._41 },
			{ rVP._12, rVP._22, rVP._32, rVP._42 },
			{ rVP._14 - rVP._13, rVP._24 - rVP._23, rVP._34 - rVP._33, rVP._44 - rVP._43 },
		};
		apDevice->SetPixelShaderConstantF(OVERLAY_CONSTANT_REGISTER, fConstants[0], OVERLAY_CONSTANT_COUNT);

		// The ground samplers are outside the game's texture stage cache too. Save and restore them on the device.
		static constexpr D3DSAMPLERSTATETYPE eSamplerStates[] = { D3DSAMP_MINFILTER, D3DSAMP_MAGFILTER, D3DSAMP_MIPFILTER, D3DSAMP_ADDRESSU, D3DSAMP_ADDRESSV, D3DSAMP_SRGBTEXTURE };
		static constexpr DWORD uiSamplerValues[] = { D3DTEXF_POINT, D3DTEXF_POINT, D3DTEXF_NONE, D3DTADDRESS_CLAMP, D3DTADDRESS_CLAMP, FALSE };
		IDirect3DBaseTexture9* const pGroundTextures[GROUND_SAMPLER_COUNT] = { apGroundTexture, apPointTexture };
		DWORD uiSavedSamplerValues[GROUND_SAMPLER_COUNT][ARRAYSIZE(eSamplerStates)];
		IDirect3DBaseTexture9* pSavedTextures[GROUND_SAMPLER_COUNT] = {};
		for (uint32_t uiSampler = 0; uiSampler < GROUND_SAMPLER_COUNT; uiSampler++) {
			const uint32_t uiStage = GROUND_BUFFER_SAMPLER + uiSampler;
			apDevice->GetTexture(uiStage, &pSavedTextures[uiSampler]);
			for (uint32_t i = 0; i < ARRAYSIZE(eSamplerStates); i++) {
				apDevice->GetSamplerState(uiStage, eSamplerStates[i], &uiSavedSamplerValues[uiSampler][i]);
				apDevice->SetSamplerState(uiStage, eSamplerStates[i], uiSamplerValues[i]);
			}
			apDevice->SetTexture(uiStage, pGroundTextures[uiSampler]);
		}

		// Depth Resolve's RESZ resolve binds the depth texture to stage 0 behind the game's texture cache.
		// Clear the cached terrain stages so every replayed pass binds its own textures.
		for (uint32_t i = 0; i < TERRAIN_SAMPLER_COUNT; i++)
			pRenderState->SetTexture(i, nullptr);

		uint32_t uiPasses = 0;
		{
			ForcedRenderStates kStates(pRenderState);
			kStates.Force(D3DRS_ZENABLE, D3DZB_FALSE);
			kStates.Force(D3DRS_ZWRITEENABLE, FALSE);
			// Draw only over pixels of statics tagged in the opaque stage. D3D9 runs the stencil test with the depth test off.
			// The first overlay fragment drawn on a sample clears its tag. Only one terrain layer blends there.
			// Fragments discarded by `clip` leave the tag for the layers behind them.
			if (Settings::bStencilTags) {
				kStates.Force(D3DRS_STENCILENABLE, TRUE);
				kStates.Force(D3DRS_TWOSIDEDSTENCILMODE, FALSE);
				kStates.Force(D3DRS_STENCILFUNC, D3DCMP_EQUAL);
				kStates.Force(D3DRS_STENCILREF, StencilTags::TERRAIN_BLEND_BIT);
				kStates.Force(D3DRS_STENCILMASK, StencilTags::TERRAIN_BLEND_BIT);
				kStates.Force(D3DRS_STENCILWRITEMASK, StencilTags::TERRAIN_BLEND_BIT);
				kStates.Force(D3DRS_STENCILPASS, D3DSTENCILOP_ZERO);
				kStates.Force(D3DRS_STENCILFAIL, D3DSTENCILOP_KEEP);
				kStates.Force(D3DRS_STENCILZFAIL, D3DSTENCILOP_KEEP);
			}
			else {
				kStates.Force(D3DRS_STENCILENABLE, FALSE);
			}
			kStates.Force(D3DRS_ALPHATESTENABLE, FALSE);
			kStates.Force(D3DRS_ADAPTIVETESS_Y, 0); // Alpha to coverage, NVIDIA only.
			kStates.Force(D3DRS_ALPHABLENDENABLE, TRUE);
			kStates.Force(D3DRS_SEPARATEALPHABLENDENABLE, FALSE);
			kStates.Force(D3DRS_BLENDOP, D3DBLENDOP_ADD);
			kStates.Force(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
			kStates.Force(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
			kStates.Force(D3DRS_COLORWRITEENABLE, D3DCOLORWRITEENABLE_RED | D3DCOLORWRITEENABLE_GREEN | D3DCOLORWRITEENABLE_BLUE);

			if (abGroundMode) {
				// Every pass draws a full-screen triangle instead of its block's mesh, through `DrawRedirect`.
				// Each pixel is shaded by the pass of the block its ground point lies in. The shader identifies the block by c92.
				kStates.Force(D3DRS_CULLMODE, D3DCULL_NONE);
				kStates.Force(D3DRS_SCISSORTESTENABLE, FALSE);

				DrawRedirect::ConsumeCounts();
				DrawRedirect::SetArmed(true);
				const LARGE_INTEGER kReplayStart = GetCounter();
				// Landscape blocks are only translated. Their model space is centred on the cell.
				uiPasses = TerrainPasses::Replay(apTarget, kInRange, [apDevice, &kEye](const NiGeometry* apGeometry) {
					DrawRedirect::BeginPass();
					const NiBound* pBound = apGeometry->m_pWorldBound;
					const NiPoint3& kTranslate = apGeometry->m_kWorld.m_Translate;
					const float fBlock[2][4] = {
						{
							pBound ? std::floor(pBound->m_kCenter.x / LANDSCAPE_BLOCK_SIZE) : 0.0f,
							pBound ? std::floor(pBound->m_kCenter.y / LANDSCAPE_BLOCK_SIZE) : 0.0f,
							0.0f, 0.0f,
						},
						{ kEye.x - kTranslate.x, kEye.y - kTranslate.y, kEye.z - kTranslate.z, 0.0f },
					};
					apDevice->SetPixelShaderConstantF(BLOCK_CONSTANT_REGISTER, fBlock[0], 2);
				});
				DrawRedirect::SetArmed(false);
				const double dReplayMilliseconds = GetMillisecondsSince(kReplayStart);
				const DrawRedirect::Counts kCounts = DrawRedirect::ConsumeCounts();
				Profiler::RecordReplay(uiPasses, kCounts.uiDrawn, kCounts.uiSkipped, kCounts.uiForeign, dReplayMilliseconds);

				static uint32_t uiLoggedGroundFrames = 0;
				if (uiLoggedGroundFrames < 3) {
					uiLoggedGroundFrames++;
					_MESSAGE("Ground mode: %u landscape passes replayed, %u triangles drawn, %u draws skipped, %u of them with another vertex buffer",
						uiPasses, kCounts.uiDrawn, kCounts.uiSkipped, kCounts.uiForeign);
				}
			}
			else {
				const LARGE_INTEGER kReplayStart = GetCounter();
				uiPasses = TerrainPasses::Replay(apTarget, kInRange);
				Profiler::RecordReplay(uiPasses, 0, 0, 0, GetMillisecondsSince(kReplayStart));
			}
		}

		const float fZero[OVERLAY_CONSTANT_COUNT][4] = {};
		apDevice->SetPixelShaderConstantF(OVERLAY_CONSTANT_REGISTER, fZero[0], OVERLAY_CONSTANT_COUNT);

		for (uint32_t uiSampler = 0; uiSampler < GROUND_SAMPLER_COUNT; uiSampler++) {
			const uint32_t uiStage = GROUND_BUFFER_SAMPLER + uiSampler;
			apDevice->SetTexture(uiStage, pSavedTextures[uiSampler]);
			if (pSavedTextures[uiSampler])
				pSavedTextures[uiSampler]->Release();
			for (uint32_t i = 0; i < ARRAYSIZE(eSamplerStates); i++)
				apDevice->SetSamplerState(uiStage, eSamplerStates[i], uiSavedSamplerValues[uiSampler][i]);
		}

		return uiPasses;
	}

	// Draws the full-screen debug view for the current debug mode: depth bands, stencil tags or ground height.
	// The given texture is the scene depth, or the ground buffer for the ground height view.
	static void DrawDebugView(IDirect3DDevice9* apDevice, const NiCamera* apCamera, IDirect3DBaseTexture9* apTexture) {
		// Capture all device state and restore it afterwards. The game's render state cache then stays valid.
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
		if (Settings::eDebugMode == Settings::DEBUG_STENCIL_TAGS || IsAttributeView()) {
			apDevice->SetRenderState(D3DRS_STENCILENABLE, TRUE);
			apDevice->SetRenderState(D3DRS_TWOSIDEDSTENCILMODE, FALSE);
			apDevice->SetRenderState(D3DRS_STENCILFUNC, D3DCMP_EQUAL);
			apDevice->SetRenderState(D3DRS_STENCILREF, StencilTags::TERRAIN_BLEND_BIT);
			apDevice->SetRenderState(D3DRS_STENCILMASK, StencilTags::TERRAIN_BLEND_BIT);
			apDevice->SetRenderState(D3DRS_STENCILWRITEMASK, 0);
		}
		else {
			apDevice->SetRenderState(D3DRS_STENCILENABLE, FALSE);
		}
		apDevice->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
		apDevice->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
		apDevice->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
		apDevice->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);
		apDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
		apDevice->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD);
		apDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
		apDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
		apDevice->SetRenderState(D3DRS_COLORWRITEENABLE, D3DCOLORWRITEENABLE_RED | D3DCOLORWRITEENABLE_GREEN | D3DCOLORWRITEENABLE_BLUE);

		apDevice->SetTexture(0, apTexture);
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

	// Logs changes in the render flow, such as frames without a post-resolve callback while the game is paused.
	// Call once per main loop iteration, before the frame is rendered.
	void OnMainLoop() {
		static constexpr uint32_t MAX_LOG_LINES = 200;

		static uint32_t uiLoop		= 0;
		static uint32_t uiLogLines	= 0;
		static bool		bRendering	= false;

		uiLoop++;
		const uint32_t uiCallbacks		= std::exchange(uiFrameCallbacks, 0);
		const uint32_t uiTaggedStages	= StencilTags::ConsumeTaggedStageCount();
		if (uiLogLines >= MAX_LOG_LINES || uiLoop == 1)
			return;

		if (uiCallbacks == 0 && bRendering) {
			_MESSAGE("Loop %u: no post-resolve callback this frame (%u tagged stages), world rendering paused?", uiLoop, uiTaggedStages);
			uiLogLines++;
		}
		else if (uiCallbacks && !bRendering) {
			_MESSAGE("Loop %u: post-resolve callbacks resumed (%u callbacks, %u tagged stages)", uiLoop, uiCallbacks, uiTaggedStages);
			uiLogLines++;
		}
		else if (uiCallbacks > 1 || uiTaggedStages > 1 || uiTaggedStages != uiCallbacks) {
			_MESSAGE("Loop %u: %u post-resolve callbacks, %u tagged stages", uiLoop, uiCallbacks, uiTaggedStages);
			uiLogLines++;
		}
		bRendering = uiCallbacks != 0;
	}

	// Renders the ground buffer, the terrain overlay and the debug views for the frame, then clears the stencil tags.
	void __cdecl OnPostResolveDepth(BSShaderAccumulator* apAccumulator, BSRenderedTexture* apTarget, IDirect3DBaseTexture9* apDepthTexture) {
		uiFrameCallbacks++;

		if (!Settings::bEnabled || !apAccumulator || !apAccumulator->m_pkCamera || !apDepthTexture) {
			TerrainPasses::Clear();
			return;
		}

		IDirect3DDevice9* pDevice = NiDX9Renderer::GetSingleton()->GetD3DDevice();
		Profiler::BeginFrame(pDevice);

		// Without tags from this frame, the stencil buffer holds nothing for the overlay to test against.
		const bool bTagged = StencilTags::HasTags(apTarget);
		const bool bOverlay = bOverlayShadersReady && (bTagged || !Settings::bStencilTags);

		// Copy the scene camera's matrices before the ground buffer pass changes them.
		NiDX9Renderer* pRenderer = NiDX9Renderer::GetSingleton();
		D3DXMATRIX kViewProjection;
		D3DXMatrixMultiply(&kViewProjection, pRenderer->GetD3DView(), pRenderer->GetD3DProj());

		const bool bGroundMode = IsGroundMode();
		const bool bAttributeView = bGroundMode && IsAttributeView();

		IDirect3DTexture9* pGroundTexture = nullptr;
		if (bOverlay || Settings::eDebugMode == Settings::DEBUG_GROUND_HEIGHT)
			pGroundTexture = GroundBuffer::Render(pDevice, apAccumulator->m_pkCamera, apTarget, apDepthTexture, bGroundMode);

		// Draw before the overlay. The overlay clears the tags it draws over.
		if (bDebugReady && Settings::eDebugMode == Settings::DEBUG_STENCIL_TAGS && bTagged)
			DrawDebugView(pDevice, apAccumulator->m_pkCamera, apDepthTexture);
		else if (bDebugReady && bAttributeView && bTagged && pGroundTexture)
			DrawDebugView(pDevice, apAccumulator->m_pkCamera, GroundBuffer::GetPointTexture());

		uint32_t uiPasses = 0;
		if (bOverlay && pGroundTexture && !bAttributeView)
			uiPasses = RenderTerrainOverlay(pDevice, apAccumulator->m_pkCamera, apTarget, pGroundTexture, GroundBuffer::GetPointTexture(), kViewProjection, bGroundMode);
		Profiler::EndStage(Profiler::STAGE_OVERLAY);
		TerrainPasses::Clear();

		if (bDebugReady && Settings::eDebugMode == Settings::DEBUG_DEPTH_BANDS)
			DrawDebugView(pDevice, apAccumulator->m_pkCamera, apDepthTexture);
		else if (bDebugReady && Settings::eDebugMode == Settings::DEBUG_GROUND_HEIGHT && pGroundTexture)
			DrawDebugView(pDevice, apAccumulator->m_pkCamera, pGroundTexture);

		StencilTags::Clear(pDevice);
		const StencilTags::GeometryCounts kTagCounts = StencilTags::ConsumeGeometryCounts();
		Profiler::RecordTags(kTagCounts.uiTagged, kTagCounts.uiDistant);
		Profiler::EndFrame();

		if (uiLoggedFrames < 3) {
			const NiFrustum& kFrustum = apAccumulator->m_pkCamera->m_kViewFrustum;
			_MESSAGE("Post-resolve callback: target %p, depth %p, near %.2f, far %.2f, stencil tags %u, %u landscape passes replayed", apTarget, apDepthTexture, kFrustum.m_fNear, kFrustum.m_fFar, bTagged, uiPasses);
			uiLoggedFrames++;
		}
	}

}
