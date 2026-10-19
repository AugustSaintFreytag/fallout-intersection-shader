#include "DrawRedirect.hpp"

#include "Dependencies.hpp"
#include "PassLog.hpp"
#include "StencilTags.hpp"

#include "Gamebryo/NiD3DVertexShader.hpp"
#include "Gamebryo/NiDX9Renderer.hpp"

#include <unordered_map>
#include <vector>

// Changes the indexed draws of replayed landscape passes on the device: the pixel shader, and for the overlay the mesh.
// Outside the replays, it keeps draws without depth writes from changing the stencil tags (`StencilTags::KeepsTags`).
// In the feature replay of toppers, it draws each object mesh with the topper feature shader (`DrawWithFeatureShader`).
// While a deferred topper pass is set up in the opaque stage, it skips the draws of the pass.
//
// The overlay shades every tagged pixel with the landscape block its ground point lies in, using that block's
// textures and constants. The replayed pass of the block binds all of them. Its mesh and pixel shader do not fit.
// The mesh covers the terrain behind the statics, not the statics on the block. In full-screen mode, the first
// `DrawIndexedPrimitive` of each replayed pass draws a full-screen triangle with the ground quad vertex shader and the
// pass's ground shader instead. In shader mode, every draw keeps its mesh and only swaps the pixel shader.
// The height field replay uses it for the field shader. The hook restores everything it changes after the draw.
// The game's state caches never see a change.
//
// A pass can draw its mesh in several pieces. Smooth Terrain's subdivided blocks take 15-19 draw calls per pass.
// In full-screen mode the triangle already covers every pixel of the block, so the hook skips the other draws of the
// pass. It counts skipped draws whose vertex buffer differs from the first draw's. Such a draw would be another mesh.
//
// Reference at "Shaders/Ground/GroundQuad.hlsl".
// Reference at "Plugin/Source/GroundShaders.cpp".
namespace DrawRedirect {

	// Slot of `IDirect3DDevice9::DrawIndexedPrimitive` in the device vtable.
	static constexpr uint32_t DRAW_INDEXED_PRIMITIVE_SLOT = 82;

	typedef HRESULT(STDMETHODCALLTYPE* DrawIndexedPrimitiveFn)(IDirect3DDevice9*, D3DPRIMITIVETYPE, INT, UINT, UINT, UINT, UINT);

	struct Vertex {
		float x, y, z;
	};

	// Covers the whole clip space square. Every pixel is drawn once.
	static constexpr Vertex kTriangle[3] = {
		{ -1.0f, 1.0f, 0.0f },
		{ 3.0f, 1.0f, 0.0f },
		{ -1.0f, -3.0f, 0.0f },
	};

	static DrawIndexedPrimitiveFn pOriginal = nullptr;
	static NiD3DVertexShader* pVertexShader = nullptr;
	static IDirect3DVertexDeclaration9* pDeclaration = nullptr;
	static Mode eMode = MODE_OFF;
	static IDirect3DPixelShader9* pPassShader = nullptr; // Pixel shader for the draws of the current pass
	static bool bPassDrawn = false; // The current pass has drawn its triangle.
	static IDirect3DVertexBuffer9* pPassStream = nullptr; // Stream 0 of that draw, compared only, no reference held
	static uint32_t uiCount = 0;
	static uint32_t uiSkipped = 0;
	static uint32_t uiForeign = 0;

	// Sampler Layout of Object Pixel Shaders
	// The feature shader reads the base map at s0 and the height map at s3. Sampler stages keep the textures of earlier
	// draws, so the device cannot tell whether a pass binds a height map. The pass's pixel shader declares it.
	// Object shaders place the height map by variant. Parallax Shaders – Objects has it at s3 in `PAR2009.pso` and
	// `PAR2029.pso` (1 and 2 lights) and at s2 in `PAR2017.pso` (3 lights, attenuation map at s4).
	static constexpr uint32_t BASE_MAP_STAGE = 0;
	static constexpr uint32_t HEIGHT_MAP_STAGE = 3;
	static constexpr uint32_t FEATURE_CONSTANT_REGISTER = 0; // c0, `FeatureParams` of "Shaders/Toppers/TopperFeatures.hlsl"
	static constexpr int32_t NO_STAGE = -1;

	struct SamplerLayout {
		int32_t iBaseMap; // Stage of `BaseMap`, or `NO_STAGE`
		int32_t iHeightMap; // Stage of `HeightMap`, or `NO_STAGE`
	};

	// Sampler states that go with a texture bound to the stage the feature shader reads.
	static constexpr D3DSAMPLERSTATETYPE kCopiedSamplerStates[] = {
		D3DSAMP_ADDRESSU, D3DSAMP_ADDRESSV, D3DSAMP_MAGFILTER, D3DSAMP_MINFILTER, D3DSAMP_MIPFILTER, D3DSAMP_MIPMAPLODBIAS,
		D3DSAMP_MAXMIPLEVEL, D3DSAMP_MAXANISOTROPY, D3DSAMP_SRGBTEXTURE,
	};

	// The texture and sampler states of a texture stage on the device. Holds a reference to the texture.
	struct StageContent {
		IDirect3DBaseTexture9* pTexture = nullptr;
		DWORD uiStates[ARRAYSIZE(kCopiedSamplerStates)] = {};

		StageContent() = default;
		StageContent(const StageContent&) = delete;
		StageContent& operator=(const StageContent&) = delete;

		~StageContent() {
			if (pTexture) {
				pTexture->Release();
			}
		}

		void Read(IDirect3DDevice9* apDevice, uint32_t auiStage) {
			apDevice->GetTexture(auiStage, &pTexture);

			for (uint32_t i = 0; i < ARRAYSIZE(kCopiedSamplerStates); i++) {
				apDevice->GetSamplerState(auiStage, kCopiedSamplerStates[i], &uiStates[i]);
			}
		}

		void Write(IDirect3DDevice9* apDevice, uint32_t auiStage) const {
			apDevice->SetTexture(auiStage, pTexture);

			for (uint32_t i = 0; i < ARRAYSIZE(kCopiedSamplerStates); i++) {
				apDevice->SetSamplerState(auiStage, kCopiedSamplerStates[i], uiStates[i]);
			}
		}
	};

	static std::unordered_map<const IDirect3DPixelShader9*, SamplerLayout> kSamplerLayouts;

	template <typename T>
	static void SafeRelease(T*& apObject) {
		if (apObject) {
			apObject->Release();
			apObject = nullptr;
		}
	}

	// Draws the given mesh with the pixel shader of the current pass.
	static HRESULT DrawWithPassShader(IDirect3DDevice9* apDevice, D3DPRIMITIVETYPE aeType, INT aiBaseVertexIndex, UINT auiMinVertexIndex, UINT auiNumVertices, UINT auiStartIndex, UINT auiPrimitiveCount) {
		IDirect3DPixelShader9* pSavedPixelShader = nullptr;
		apDevice->GetPixelShader(&pSavedPixelShader);
		apDevice->SetPixelShader(pPassShader);
		const HRESULT hResult = pOriginal(apDevice, aeType, aiBaseVertexIndex, auiMinVertexIndex, auiNumVertices, auiStartIndex, auiPrimitiveCount);
		apDevice->SetPixelShader(pSavedPixelShader);
		SafeRelease(pSavedPixelShader);
		return hResult;
	}

	// Returns the stage of a sampler that the constant table of a pixel shader declares by name.
	// Returns `NO_STAGE` if the shader declares no such sampler.
	static int32_t GetSamplerStage(ID3DXConstantTable* apTable, const char* apName) {
		const D3DXHANDLE hConstant = apTable->GetConstantByName(nullptr, apName);

		if (!hConstant) {
			return NO_STAGE;
		}

		return static_cast<int32_t>(apTable->GetSamplerIndex(hConstant));
	}

	// Returns where the given object pixel shader reads its base map and height map, from its constant table.
	// The game's shaders keep their constant tables. A shader without one reads neither.
	static SamplerLayout GetSamplerLayout(IDirect3DPixelShader9* apShader) {
		const auto it = kSamplerLayouts.find(apShader);

		if (it != kSamplerLayouts.end()) {
			return it->second;
		}

		SamplerLayout kLayout = { NO_STAGE, NO_STAGE };
		UINT uiSize = 0;

		if (apShader && SUCCEEDED(apShader->GetFunction(nullptr, &uiSize)) && uiSize) {
			std::vector<uint8_t> kFunction(uiSize);
			ID3DXConstantTable* pTable = nullptr;

			if (SUCCEEDED(apShader->GetFunction(kFunction.data(), &uiSize))
				&& SUCCEEDED(D3DXGetShaderConstantTable(reinterpret_cast<const DWORD*>(kFunction.data()), &pTable))) {
				kLayout.iBaseMap = GetSamplerStage(pTable, "BaseMap");
				kLayout.iHeightMap = GetSamplerStage(pTable, "HeightMap");
				pTable->Release();
			}
		}
		kSamplerLayouts[apShader] = kLayout;
		return kLayout;
	}

	// Draws the given mesh with the topper feature shader, telling it whether the pass binds a height map.
	// Moves the base map and height map to the stages the feature shader reads for the draw.
	// Skips draws whose pixel shader reads no base map. The features there stay neutral.
	static HRESULT DrawWithFeatureShader(IDirect3DDevice9* apDevice, D3DPRIMITIVETYPE aeType, INT aiBaseVertexIndex, UINT auiMinVertexIndex, UINT auiNumVertices, UINT auiStartIndex, UINT auiPrimitiveCount) {
		IDirect3DPixelShader9* pSavedPixelShader = nullptr;
		apDevice->GetPixelShader(&pSavedPixelShader);
		const SamplerLayout kLayout = GetSamplerLayout(pSavedPixelShader);

		if (kLayout.iBaseMap == NO_STAGE) {
			SafeRelease(pSavedPixelShader);
			return D3D_OK;
		}

		const bool bHeightMap = kLayout.iHeightMap != NO_STAGE;

		// Both maps are read before either is bound. A map can sit on the stage the other one moves to.
		StageContent kBaseMap;
		StageContent kHeightMap;
		kBaseMap.Read(apDevice, kLayout.iBaseMap);

		if (bHeightMap) {
			kHeightMap.Read(apDevice, kLayout.iHeightMap);
		}

		StageContent kSavedBase;
		StageContent kSavedHeight;
		kSavedBase.Read(apDevice, BASE_MAP_STAGE);
		kSavedHeight.Read(apDevice, HEIGHT_MAP_STAGE);

		kBaseMap.Write(apDevice, BASE_MAP_STAGE);

		if (bHeightMap) {
			kHeightMap.Write(apDevice, HEIGHT_MAP_STAGE);
		}

		float fHeightMapFlag = 0.0f;

		if (bHeightMap) {
			fHeightMapFlag = 1.0f;
		}

		float fSavedConstant[4];
		apDevice->GetPixelShaderConstantF(FEATURE_CONSTANT_REGISTER, fSavedConstant, 1);

		const float fFeatureParams[4] = { fHeightMapFlag, 0.0f, 0.0f, 0.0f };
		apDevice->SetPixelShaderConstantF(FEATURE_CONSTANT_REGISTER, fFeatureParams, 1);
		apDevice->SetPixelShader(pPassShader);

		const HRESULT hResult = pOriginal(apDevice, aeType, aiBaseVertexIndex, auiMinVertexIndex, auiNumVertices, auiStartIndex, auiPrimitiveCount);

		apDevice->SetPixelShader(pSavedPixelShader);
		apDevice->SetPixelShaderConstantF(FEATURE_CONSTANT_REGISTER, fSavedConstant, 1);
		kSavedBase.Write(apDevice, BASE_MAP_STAGE);
		kSavedHeight.Write(apDevice, HEIGHT_MAP_STAGE);
		SafeRelease(pSavedPixelShader);

		return hResult;
	}

	// Draws according to the mode: unchanged, with the pixel shader of the current pass, or as the full-screen triangle
	// in place of the first indexed draw of a pass. Full-screen mode skips the other draws of the pass.
	static HRESULT STDMETHODCALLTYPE DrawIndexedPrimitiveHook(IDirect3DDevice9* apDevice, D3DPRIMITIVETYPE aeType, INT aiBaseVertexIndex, UINT auiMinVertexIndex, UINT auiNumVertices, UINT auiStartIndex, UINT auiPrimitiveCount) {
		if (PassLog::IsCapturing()) {
			PassLog::OnDraw(apDevice);
		}

		if (eMode == MODE_SKIP) {
			uiSkipped++;
			return D3D_OK;
		}

		if (eMode == MODE_OFF) {
			if (!StencilTags::KeepsTags()) {
				return pOriginal(apDevice, aeType, aiBaseVertexIndex, auiMinVertexIndex, auiNumVertices, auiStartIndex, auiPrimitiveCount);
			}

			// Set on the device directly. The tagging window forces the write mask in the render state cache.
			apDevice->SetRenderState(D3DRS_STENCILWRITEMASK, 0);
			const HRESULT hResult = pOriginal(apDevice, aeType, aiBaseVertexIndex, auiMinVertexIndex, auiNumVertices, auiStartIndex, auiPrimitiveCount);
			apDevice->SetRenderState(D3DRS_STENCILWRITEMASK, StencilTags::TAG_BITS);
			return hResult;
		}
		// With the terrain pixel shader, the pass would paint plain terrain over the target.
		if (!pPassShader) {
			return D3D_OK;
		}

		if (eMode == MODE_TOPPER_FEATURES) {
			return DrawWithFeatureShader(apDevice, aeType, aiBaseVertexIndex, auiMinVertexIndex, auiNumVertices, auiStartIndex, auiPrimitiveCount);
		}

		if (eMode == MODE_SHADER) {
			return DrawWithPassShader(apDevice, aeType, aiBaseVertexIndex, auiMinVertexIndex, auiNumVertices, auiStartIndex, auiPrimitiveCount);
		}

		IDirect3DVertexShader9* pSavedShader = nullptr;
		IDirect3DPixelShader9* pSavedPixelShader = nullptr;
		IDirect3DVertexDeclaration9* pSavedDeclaration = nullptr;
		IDirect3DVertexBuffer9* pSavedStream = nullptr;
		UINT uiSavedOffset = 0;
		UINT uiSavedStride = 0;
		apDevice->GetStreamSource(0, &pSavedStream, &uiSavedOffset, &uiSavedStride);

		if (bPassDrawn) {
			uiSkipped++;

			if (pSavedStream != pPassStream) {
				uiForeign++;
			}

			SafeRelease(pSavedStream);
			return D3D_OK;
		}
		bPassDrawn = true;
		pPassStream = pSavedStream;

		apDevice->GetVertexShader(&pSavedShader);
		apDevice->GetPixelShader(&pSavedPixelShader);
		apDevice->GetVertexDeclaration(&pSavedDeclaration);

		apDevice->SetVertexDeclaration(pDeclaration);
		apDevice->SetVertexShader(pVertexShader->GetShaderHandle());
		apDevice->SetPixelShader(pPassShader);
		const HRESULT hResult = apDevice->DrawPrimitiveUP(D3DPT_TRIANGLELIST, 1, kTriangle, sizeof(Vertex));

		// `DrawPrimitiveUP` leaves stream 0 unset.
		apDevice->SetStreamSource(0, pSavedStream, uiSavedOffset, uiSavedStride);
		apDevice->SetVertexDeclaration(pSavedDeclaration);
		apDevice->SetVertexShader(pSavedShader);
		apDevice->SetPixelShader(pSavedPixelShader);
		SafeRelease(pSavedStream);
		SafeRelease(pSavedDeclaration);
		SafeRelease(pSavedShader);
		SafeRelease(pSavedPixelShader);

		uiCount++;
		return hResult;
	}

	// Loads the ground quad vertex shader and hooks `DrawIndexedPrimitive` on the device.
	// Call once the D3D device exists, at NVSE DeferredInit.
	void Initialize() {
		pVertexShader = Dependencies::CreateVertexShader("IntersectionBlend\\GroundQuad.vso");

		if (!pVertexShader) {
			_MESSAGE("Failed to load the ground quad vertex shader, terrain overlay disabled");
			return;
		}
		pVertexShader->IncRefCount();

		IDirect3DDevice9* pDevice = NiDX9Renderer::GetSingleton()->GetD3DDevice();
		const D3DVERTEXELEMENT9 kElements[] = {
			{ 0, 0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
			D3DDECL_END()
		};

		if (FAILED(pDevice->CreateVertexDeclaration(kElements, &pDeclaration))) {
			_MESSAGE("Failed to create the ground quad vertex declaration, terrain overlay disabled");
			return;
		}

		// Chain to the current slot entry. Another module may have hooked it first.
		void** ppVTable = *reinterpret_cast<void***>(pDevice);
		pOriginal = reinterpret_cast<DrawIndexedPrimitiveFn>(ppVTable[DRAW_INDEXED_PRIMITIVE_SLOT]);
		SafeWrite32(reinterpret_cast<SIZE_T>(&ppVTable[DRAW_INDEXED_PRIMITIVE_SLOT]), reinterpret_cast<SIZE_T>(&DrawIndexedPrimitiveHook));
		_MESSAGE("Draw redirect installed (device %p, vtable %p, slot %u, previous %p)", pDevice, ppVTable, DRAW_INDEXED_PRIMITIVE_SLOT, pOriginal);
	}

	bool IsReady() {
		return pOriginal != nullptr;
	}

	void SetMode(Mode aeMode) {
		eMode = MODE_OFF;

		if (IsReady()) {
			eMode = aeMode;
		}

		pPassShader = nullptr;
		bPassDrawn = false;
	}

	// Sets the pixel shader for the draws of the next replayed pass. In full-screen mode, also lets the next draw call
	// draw the triangle. Call before each replayed pass. Without a shader, the draws of the pass are skipped.
	void BeginPass(IDirect3DPixelShader9* apPixelShader) {
		pPassShader = apPixelShader;
		bPassDrawn = false;
		pPassStream = nullptr;
	}

	// Returns the draw counts since the last call and resets them: triangles drawn, draws skipped, and skipped draws
	// with another vertex buffer than the first draw of their pass.
	Counts ConsumeCounts() {
		const Counts kResult = { uiCount, uiSkipped, uiForeign };
		uiCount = 0;
		uiSkipped = 0;
		uiForeign = 0;
		return kResult;
	}

}
