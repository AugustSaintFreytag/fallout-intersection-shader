#include "Dependencies.hpp"

namespace Dependencies {

	static constexpr const char*	PLUGIN_TITLE			= "Intersection Blend";
	static constexpr uint32_t		uiShaderLoaderVersion	= 131;

	typedef void(__cdecl* RegisterPostResolveDepthCallbackFn)(PostResolveDepthCallback);
	typedef NiD3DVertexShader*(__cdecl* CreateVertexShaderFn)(const char*);
	typedef NiD3DPixelShader*(__cdecl* CreatePixelShaderFn)(const char*);

	static RegisterPostResolveDepthCallbackFn	pRegisterPostResolveDepthCallback	= nullptr;
	static CreateVertexShaderFn					pCreateVertexShader					= nullptr;
	static CreatePixelShaderFn					pCreatePixelShader					= nullptr;

	static bool Fail(const char* apMessage) {
		_MESSAGE("Missing dependency: %s", apMessage);
		MessageBox(NULL, apMessage, PLUGIN_TITLE, MB_OK | MB_ICONERROR);
		return false;
	}

	// Checks that every hard dependency is loaded and resolves their exports.
	// Shows a message box and returns false if one is missing or outdated.
	bool Check(const NVSEInterface* apNVSE) {
		HMODULE hShaderLoader	= GetModuleHandle("Fallout Shader Loader.dll");
		HMODULE hLODFlickerFix	= GetModuleHandle("LODFlickerFix.dll");
		HMODULE hDepthResolve	= GetModuleHandle("DepthResolve.dll");
		HMODULE hVPT			= GetModuleHandle("VanillaPlusTerrain.dll");

		if (!hShaderLoader)
			return Fail("Fallout Shader Loader not found.\nIntersection Blend cannot be used without it and is disabled.");

		auto pQuery = reinterpret_cast<_NVSEPlugin_Query>(GetProcAddress(hShaderLoader, "NVSEPlugin_Query"));
		PluginInfo kInfo = {};
		if (!pQuery || !pQuery(apNVSE, &kInfo) || kInfo.version < uiShaderLoaderVersion) {
			char cBuffer[192];
			sprintf_s(cBuffer, "Fallout Shader Loader is outdated.\nCurrent version: %u\nMinimum required version: %u\nIntersection Blend is disabled.", kInfo.version, uiShaderLoaderVersion);
			return Fail(cBuffer);
		}

		pCreateVertexShader = reinterpret_cast<CreateVertexShaderFn>(GetProcAddress(hShaderLoader, "CreateVertexShader"));
		pCreatePixelShader	= reinterpret_cast<CreatePixelShaderFn>(GetProcAddress(hShaderLoader, "CreatePixelShader"));
		if (!pCreateVertexShader || !pCreatePixelShader)
			return Fail("Fallout Shader Loader does not export its shader creation functions.\nIntersection Blend is disabled.");

		// Depth math assumes the reversed depth buffer of LOD Flicker Fix.
		if (!hLODFlickerFix)
			return Fail("LOD Flicker Fix not found.\nIntersection Blend cannot be used without it and is disabled.");

		if (!hDepthResolve)
			return Fail("Depth Resolve not found.\nIntersection Blend cannot be used without it and is disabled.");

		pRegisterPostResolveDepthCallback = reinterpret_cast<RegisterPostResolveDepthCallbackFn>(GetProcAddress(hDepthResolve, "RegisterPostResolveDepthCallback"));
		if (!pRegisterPostResolveDepthCallback)
			return Fail("Depth Resolve does not provide RegisterPostResolveDepthCallback.\nA Depth Resolve build with the post-resolve callback is required.\nIntersection Blend is disabled.");

		_MESSAGE("Dependencies OK (Shader Loader %u, Vanilla Plus Terrain %s)", kInfo.version, hVPT ? "found" : "not found");
		return true;
	}

	void RegisterPostResolveDepthCallback(PostResolveDepthCallback apCallback) {
		pRegisterPostResolveDepthCallback(apCallback);
	}

	// Loads a vertex shader from `Data\Shaders\Loose\` through Shader Loader.
	NiD3DVertexShader* CreateVertexShader(const char* apFilename) {
		return pCreateVertexShader(apFilename);
	}

	// Loads a pixel shader from `Data\Shaders\Loose\` through Shader Loader.
	NiD3DPixelShader* CreatePixelShader(const char* apFilename) {
		return pCreatePixelShader(apFilename);
	}

}
