#pragma once

class NiCamera;

namespace TopperBlend {

	void Initialize();

	bool CopyBase(IDirect3DDevice9* apDevice, IDirect3DBaseTexture9* apDepthTexture);

	void MarkBases(IDirect3DDevice9* apDevice);

	void Composite(IDirect3DDevice9* apDevice, const NiCamera* apCamera, IDirect3DBaseTexture9* apDepthTexture);

}
