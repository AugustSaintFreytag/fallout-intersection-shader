#pragma once

class NiCamera;

namespace TopperCrop {

	void Initialize();

	bool CopyBaseDepth(IDirect3DDevice9* apDevice, IDirect3DBaseTexture9* apDepthTexture);

	void Crop(IDirect3DDevice9* apDevice, const NiCamera* apCamera, IDirect3DBaseTexture9* apDepthTexture);

}
