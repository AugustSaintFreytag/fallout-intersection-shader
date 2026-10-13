#pragma once

#include "nvse/PluginAPI.h"

class BSShaderAccumulator;
class BSRenderedTexture;
class NiD3DPixelShader;
class NiD3DVertexShader;

// Exported by our patched Depth Resolve (patches/depth-resolve-post-resolve-callback.patch).
// Called right after the pre-water depth resolve, while the scene render target is still bound.
typedef void(__cdecl* PostResolveDepthCallback)(BSShaderAccumulator* apAccumulator, BSRenderedTexture* apTarget, IDirect3DBaseTexture9* apDepthTexture);

namespace Dependencies {
	// Shows a message box and returns false if a hard dependency is missing.
	bool Check(const NVSEInterface* apNVSE);

	void RegisterPostResolveDepthCallback(PostResolveDepthCallback apCallback);

	// Loads from Data\Shaders\Loose\ through Shader Loader.
	NiD3DVertexShader*	CreateVertexShader(const char* apFilename);
	NiD3DPixelShader*	CreatePixelShader(const char* apFilename);
}
