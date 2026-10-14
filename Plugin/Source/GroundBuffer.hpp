#pragma once

class BSRenderedTexture;
class NiCamera;

namespace GroundBuffer {

	void Initialize();

	IDirect3DTexture9* Render(IDirect3DDevice9* apDevice, const NiCamera* apCamera, const BSRenderedTexture* apTarget, IDirect3DBaseTexture9* apDepthTexture, bool abGroundMode);

	IDirect3DTexture9* GetPointTexture();

}
