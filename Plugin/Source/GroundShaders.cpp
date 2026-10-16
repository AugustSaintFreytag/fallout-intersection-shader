#include "GroundShaders.hpp"

#include "Dependencies.hpp"
#include "TerrainPasses.hpp"

#include "Gamebryo/NiD3DPixelShader.hpp"

#include <algorithm>
#include <cstring>
#include <vector>

// Loads the pixel shaders that `DrawRedirect` draws in place of the terrain pixel shaders during the replays.
//
// The overlay replay draws every landscape pass with a ground shader. It shades from the ground buffers and lights
// like the installed terrain shaders. There is one set of 28 per terrain shader mod. The installed `SLS2092.pso`
// decides which set loads. The height field replay draws every landscape pass with the field shader.
// The installed terrain shaders stay untouched.
//
// Reference at "Shaders/compile.ps1".
namespace GroundShaders {

	static constexpr uint32_t TEXTURE_COUNTS = 7;
	static constexpr uint32_t LIGHT_VARIANTS = 4;
	static constexpr uint32_t kPointLights[LIGHT_VARIANTS] = { 0, 6, 12, 24 };

	// Passes per texture count: no point lights, 6, 12 and 24, each with and without canopy shadows.
	// Both canopy variants draw with the same pixel shader.
	static constexpr uint32_t PASSES_PER_TEXTURE_COUNT = 8;

	// Ground shader sets, by folder in `Shaders\Loose\IntersectionBlend\Ground\`.
	enum ShaderSet : int32_t {
		SET_NONE = -1,
		SET_VPT = 0,
		SET_PARALLAX = 1,
		SET_PARALLAX_OPTIMIZED = 2,
	};

	static constexpr const char* kSetFolders[] = { "VPT", "Parallax", "ParallaxOptimized" };
	static constexpr const char* kSetNames[] = { "Vanilla Plus Terrain", "Parallax Shaders", "Parallax Shaders (Optimized)" };

	static constexpr const char* INSTALLED_SHADER_PATH = "Data\\Shaders\\Loose\\SLS2092.pso";

	static NiD3DPixelShader* pOverlayShaders[TEXTURE_COUNTS * LIGHT_VARIANTS] = {};
	static NiD3DPixelShader* pFieldShader = nullptr;

	static bool ReadFile(const char* apPath, std::vector<char>& arData) {
		FILE* pFile = nullptr;
		if (fopen_s(&pFile, apPath, "rb") || !pFile)
			return false;

		char cBuffer[4096];
		size_t uiRead;
		while ((uiRead = fread(cBuffer, 1, sizeof(cBuffer), pFile)) > 0)
			arData.insert(arData.end(), cBuffer, cBuffer + uiRead);
		fclose(pFile);
		return true;
	}

	static bool Contains(const std::vector<char>& arData, const char* apText) {
		return std::search(arData.begin(), arData.end(), apText, apText + strlen(apText)) != arData.end();
	}

	// Returns whether a compiled shader holds the given value in one of its literal constants (`def`).
	static bool HasLiteral(const std::vector<char>& arCode, float afValue) {
		const uint32_t* pTokens = reinterpret_cast<const uint32_t*>(arCode.data());
		const size_t uiTokenCount = arCode.size() / sizeof(uint32_t);

		// Token 0 is the version.
		for (size_t i = 1; i < uiTokenCount;) {
			const uint32_t uiToken = pTokens[i];
			const uint32_t uiOpcode = uiToken & D3DSI_OPCODE_MASK;
			if (uiToken == D3DSIO_END)
				break;
			if (uiOpcode == D3DSIO_COMMENT) {
				i += 1 + ((uiToken & D3DSI_COMMENTSIZE_MASK) >> D3DSI_COMMENTSIZE_SHIFT);
				continue;
			}

			// Destination register, then 4 floats.
			const uint32_t uiLength = (uiToken & D3DSI_INSTLENGTH_MASK) >> D3DSI_INSTLENGTH_SHIFT;
			if (uiOpcode == D3DSIO_DEF && uiLength == 5 && i + 5 < uiTokenCount) {
				for (uint32_t uiComponent = 0; uiComponent < 4; uiComponent++) {
					float fValue;
					memcpy(&fValue, &pTokens[i + 2 + uiComponent], sizeof(fValue));
					if (fValue == afValue)
						return true;
				}
			}
			i += 1 + uiLength;
		}

		return false;
	}

	// Returns the ground shader set that matches the installed terrain shaders.
	//
	// Parallax Shaders – Terrain names its sun colour `SunColor` in the constant table. Vanilla Plus Terrain names it
	// `PSLightColor`. The Optimized build of Parallax fades parallax out at 1024 units instead of 2048.
	// Its code then holds 1 / 1024 as a literal.
	static ShaderSet DetectSet() {
		std::vector<char> kCode;
		if (!ReadFile(INSTALLED_SHADER_PATH, kCode)) {
			_MESSAGE("Terrain overlay disabled: %s not found. Vanilla Plus Terrain is required.", INSTALLED_SHADER_PATH);
			return SET_NONE;
		}

		if (Contains(kCode, "SunColor"))
			return HasLiteral(kCode, 1.0f / 1024.0f) ? SET_PARALLAX_OPTIMIZED : SET_PARALLAX;
		if (!Contains(kCode, "PSLightColor"))
			_MESSAGE("Unknown terrain shaders in %s, using the ground shaders for Vanilla Plus Terrain", INSTALLED_SHADER_PATH);
		return SET_VPT;
	}

	// Loads the ground shader set that matches the installed terrain shaders, and the field shader.
	// Returns false if any of them is missing.
	// Call once the D3D device exists, at NVSE DeferredInit.
	bool Load() {
		const ShaderSet eSet = DetectSet();
		if (eSet == SET_NONE)
			return false;

		_MESSAGE("Terrain shaders: %s", kSetNames[eSet]);

		for (uint32_t uiTextures = 0; uiTextures < TEXTURE_COUNTS; uiTextures++) {
			for (uint32_t uiLights = 0; uiLights < LIGHT_VARIANTS; uiLights++) {
				char cPath[MAX_PATH];
				sprintf_s(cPath, "IntersectionBlend\\Ground\\%s\\T%uL%u.pso", kSetFolders[eSet], uiTextures + 1, kPointLights[uiLights]);
				NiD3DPixelShader* pShader = Dependencies::CreatePixelShader(cPath);
				if (!pShader) {
					_MESSAGE("Terrain overlay disabled: failed to load the ground shader %s", cPath);
					return false;
				}
				pShader->IncRefCount();
				pOverlayShaders[uiTextures * LIGHT_VARIANTS + uiLights] = pShader;
			}
		}

		pFieldShader = Dependencies::CreatePixelShader("IntersectionBlend\\GroundField.pso");
		if (!pFieldShader) {
			_MESSAGE("Terrain overlay disabled: failed to load the field shader");
			return false;
		}
		pFieldShader->IncRefCount();

		return true;
	}

	// Returns the ground shader for a landscape pass type, or null for other pass types.
	IDirect3DPixelShader9* GetOverlayShader(uint32_t auiPassType) {
		if (auiPassType < TerrainPasses::FIRST_LANDSCAPE_PASS || auiPassType > TerrainPasses::LAST_LANDSCAPE_PASS)
			return nullptr;

		const uint32_t uiIndex = auiPassType - TerrainPasses::FIRST_LANDSCAPE_PASS;
		const uint32_t uiTextures = uiIndex / PASSES_PER_TEXTURE_COUNT;
		const uint32_t uiLights = (uiIndex % PASSES_PER_TEXTURE_COUNT) / 2;
		NiD3DPixelShader* pShader = pOverlayShaders[uiTextures * LIGHT_VARIANTS + uiLights];
		return pShader ? pShader->GetShaderHandle() : nullptr;
	}

	IDirect3DPixelShader9* GetFieldShader() {
		return pFieldShader ? pFieldShader->GetShaderHandle() : nullptr;
	}

}
