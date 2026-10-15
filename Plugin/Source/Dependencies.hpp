#pragma once

#include "nvse/PluginAPI.h"

class BSShaderAccumulator;
class BSRenderedTexture;
class NiD3DPixelShader;
class NiD3DVertexShader;

// A callback of the patched Depth Resolve, called right after the pre-water depth resolve.
// The scene render target is still bound.
//
// Reference at "Plugin/Patches/depth-resolve-post-resolve-callback.patch".
typedef void(__cdecl* PostResolveDepthCallback)(BSShaderAccumulator* apAccumulator, BSRenderedTexture* apTarget, IDirect3DBaseTexture9* apDepthTexture);

namespace Dependencies {

	bool Check(const NVSEInterface* apNVSE);

	void RegisterPostResolveDepthCallback(PostResolveDepthCallback apCallback);

	NiD3DVertexShader*	CreateVertexShader(const char* apFilename);
	NiD3DPixelShader*	CreatePixelShader(const char* apFilename);

}
