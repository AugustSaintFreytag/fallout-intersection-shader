#include "DrawRedirect.hpp"

#include "Dependencies.hpp"

#include "Gamebryo/NiD3DVertexShader.hpp"
#include "Gamebryo/NiDX9Renderer.hpp"

// Replaces indexed draws on the device with one full-screen triangle while armed.
//
// Ground mode shades every tagged pixel with the landscape block its ground point lies in, using that block's
// textures and constants. The replayed pass of the block binds all of them. Only its mesh does not fit: it covers
// the terrain behind the statics, not the statics on the block. While armed, every `DrawIndexedPrimitive` draws a
// full-screen triangle with the ground quad vertex shader instead. The hook then restores the vertex shader,
// declaration and stream 0. The game's state caches never see a change.
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
	static uint32_t						uiCount			= 0;

	template <typename T>
	static void SafeRelease(T*& apObject) {
		if (apObject) {
			apObject->Release();
			apObject = nullptr;
		}
	}

	// Draws the full-screen triangle in place of the indexed draw while armed. Passes the call through otherwise.
	static HRESULT STDMETHODCALLTYPE DrawIndexedPrimitiveHook(IDirect3DDevice9* apDevice, D3DPRIMITIVETYPE aeType, INT aiBaseVertexIndex, UINT auiMinVertexIndex, UINT auiNumVertices, UINT auiStartIndex, UINT auiPrimitiveCount) {
		if (!bArmed)
			return pOriginal(apDevice, aeType, aiBaseVertexIndex, auiMinVertexIndex, auiNumVertices, auiStartIndex, auiPrimitiveCount);

		IDirect3DVertexShader9* pSavedShader = nullptr;
		IDirect3DVertexDeclaration9* pSavedDeclaration = nullptr;
		IDirect3DVertexBuffer9* pSavedStream = nullptr;
		UINT uiSavedOffset = 0;
		UINT uiSavedStride = 0;
		apDevice->GetVertexShader(&pSavedShader);
		apDevice->GetVertexDeclaration(&pSavedDeclaration);
		apDevice->GetStreamSource(0, &pSavedStream, &uiSavedOffset, &uiSavedStride);

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
			_MESSAGE("Failed to load the ground quad vertex shader, ground mode disabled");
			return;
		}
		pVertexShader->IncRefCount();

		IDirect3DDevice9* pDevice = NiDX9Renderer::GetSingleton()->GetD3DDevice();
		const D3DVERTEXELEMENT9 kElements[] = {
			{ 0, 0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
			D3DDECL_END()
		};
		if (FAILED(pDevice->CreateVertexDeclaration(kElements, &pDeclaration))) {
			_MESSAGE("Failed to create the ground quad vertex declaration, ground mode disabled");
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
	}

	// Returns the number of draws redirected since the last call and resets it.
	uint32_t ConsumeCount() {
		const uint32_t uiResult = uiCount;
		uiCount = 0;
		return uiResult;
	}

}
