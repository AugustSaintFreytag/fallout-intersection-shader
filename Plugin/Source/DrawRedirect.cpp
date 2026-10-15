#include "DrawRedirect.hpp"

#include "Dependencies.hpp"

#include "Gamebryo/NiD3DVertexShader.hpp"
#include "Gamebryo/NiDX9Renderer.hpp"

// Replaces indexed draws on the device with one full-screen triangle while armed.
//
// The overlay shades every tagged pixel with the landscape block its ground point lies in, using that block's
// textures and constants. The replayed pass of the block binds all of them. Only its mesh does not fit: it covers
// the terrain behind the statics, not the statics on the block. While armed, the first `DrawIndexedPrimitive` of each
// replayed pass draws a full-screen triangle with the ground quad vertex shader instead. The hook then restores the
// vertex shader, declaration and stream 0. The game's state caches never see a change.
//
// A pass can draw its mesh in several pieces. Smooth Terrain's subdivided blocks take 15-19 draw calls per pass.
// The triangle already covers every pixel of the block, so the hook skips the other draws of the pass.
// It counts skipped draws whose vertex buffer differs from the first draw's. Such a draw would be another mesh.
//
// Reference at "Shaders/Ground/GroundQuad.hlsl".
namespace DrawRedirect {

	// Slot of `IDirect3DDevice9::DrawIndexedPrimitive` in the device vtable.
	static constexpr uint32_t DRAW_INDEXED_PRIMITIVE_SLOT = 82;

	typedef HRESULT(STDMETHODCALLTYPE* DrawIndexedPrimitiveFn)(IDirect3DDevice9*, D3DPRIMITIVETYPE, INT, UINT, UINT, UINT, UINT);

	struct Vertex {
		float x, y, z;
	};

	// Covers the whole clip space square. Every pixel is drawn once.
	static constexpr Vertex kTriangle[3] = {
		{ -1.0f,  1.0f, 0.0f },
		{  3.0f,  1.0f, 0.0f },
		{ -1.0f, -3.0f, 0.0f },
	};

	static DrawIndexedPrimitiveFn		pOriginal		= nullptr;
	static NiD3DVertexShader*			pVertexShader	= nullptr;
	static IDirect3DVertexDeclaration9*	pDeclaration	= nullptr;
	static bool							bArmed			= false;
	static bool							bPassDrawn		= false;	// The current pass has drawn its triangle.
	static IDirect3DVertexBuffer9*		pPassStream		= nullptr;	// Stream 0 of that draw, compared only, no reference held
	static uint32_t						uiCount			= 0;
	static uint32_t						uiSkipped		= 0;
	static uint32_t						uiForeign		= 0;

	template <typename T>
	static void SafeRelease(T*& apObject) {
		if (apObject) {
			apObject->Release();
			apObject = nullptr;
		}
	}

	// Draws the full-screen triangle in place of the first indexed draw of a pass while armed, and skips the others.
	// Passes the call through when not armed.
	static HRESULT STDMETHODCALLTYPE DrawIndexedPrimitiveHook(IDirect3DDevice9* apDevice, D3DPRIMITIVETYPE aeType, INT aiBaseVertexIndex, UINT auiMinVertexIndex, UINT auiNumVertices, UINT auiStartIndex, UINT auiPrimitiveCount) {
		if (!bArmed)
			return pOriginal(apDevice, aeType, aiBaseVertexIndex, auiMinVertexIndex, auiNumVertices, auiStartIndex, auiPrimitiveCount);

		IDirect3DVertexShader9* pSavedShader = nullptr;
		IDirect3DVertexDeclaration9* pSavedDeclaration = nullptr;
		IDirect3DVertexBuffer9* pSavedStream = nullptr;
		UINT uiSavedOffset = 0;
		UINT uiSavedStride = 0;
		apDevice->GetStreamSource(0, &pSavedStream, &uiSavedOffset, &uiSavedStride);

		if (bPassDrawn) {
			uiSkipped++;
			if (pSavedStream != pPassStream)
				uiForeign++;
			SafeRelease(pSavedStream);
			return D3D_OK;
		}
		bPassDrawn = true;
		pPassStream = pSavedStream;

		apDevice->GetVertexShader(&pSavedShader);
		apDevice->GetVertexDeclaration(&pSavedDeclaration);

		apDevice->SetVertexDeclaration(pDeclaration);
		apDevice->SetVertexShader(pVertexShader->GetShaderHandle());
		const HRESULT hResult = apDevice->DrawPrimitiveUP(D3DPT_TRIANGLELIST, 1, kTriangle, sizeof(Vertex));

		// `DrawPrimitiveUP` leaves stream 0 unset.
		apDevice->SetStreamSource(0, pSavedStream, uiSavedOffset, uiSavedStride);
		apDevice->SetVertexDeclaration(pSavedDeclaration);
		apDevice->SetVertexShader(pSavedShader);
		SafeRelease(pSavedStream);
		SafeRelease(pSavedDeclaration);
		SafeRelease(pSavedShader);

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

	void SetArmed(bool abArmed) {
		bArmed = abArmed && IsReady();
		bPassDrawn = false;
	}

	// Lets the next draw call draw the triangle. Call before each replayed pass.
	void BeginPass() {
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
