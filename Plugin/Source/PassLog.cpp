#include "PassLog.hpp"

#include "Bethesda/BSShaderProperty.hpp"
#include "Gamebryo/NiGeometry.hpp"

#include <unordered_map>
#include <vector>

// Logs the device state of every draw of a topper pass for one frame, with `[Debug] bLogTopperPasses`.
// The reload key starts the capture. It covers the frame after the key press.
//
// Toppers drawn in the opaque stage log the state the game sets up for them. Deferred toppers are set up there too,
// with their draws skipped, and log the same. Each replay logs its own state. A diff shows what a replay sets up differently.
// Shaders and textures are logged by content, so the logs of two game sessions compare.
namespace PassLog {

	static constexpr uint32_t PIXEL_CONSTANT_COUNT = 32;
	static constexpr uint32_t TEXTURE_STAGE_COUNT = 8;
	static constexpr const char* STAGE_NAMES[] = { "opaque", "depth", "colour", "features" };

	static constexpr D3DRENDERSTATETYPE kStates[] = {
		D3DRS_ZENABLE, D3DRS_ZWRITEENABLE, D3DRS_ZFUNC, D3DRS_ALPHABLENDENABLE, D3DRS_SRCBLEND, D3DRS_DESTBLEND,
		D3DRS_BLENDOP, D3DRS_SEPARATEALPHABLENDENABLE, D3DRS_SRCBLENDALPHA, D3DRS_DESTBLENDALPHA, D3DRS_ALPHATESTENABLE,
		D3DRS_ALPHAREF, D3DRS_ALPHAFUNC, D3DRS_COLORWRITEENABLE, D3DRS_CULLMODE, D3DRS_FOGENABLE, D3DRS_STENCILENABLE,
		D3DRS_STENCILFUNC, D3DRS_STENCILREF, D3DRS_SRGBWRITEENABLE,
	};
	static constexpr const char* STATE_NAMES[] = {
		"z", "zwrite", "zfunc", "blend", "src", "dst", "op", "sepalpha", "srca", "dsta", "atest", "aref", "afunc",
		"cwrite", "cull", "fog", "stencil", "sfunc", "sref", "srgbw",
	};
	static_assert(ARRAYSIZE(kStates) == ARRAYSIZE(STATE_NAMES));

	static bool bArmed = false;
	static bool bCapturing = false;
	static const BSShaderProperty::RenderPass* pPass = nullptr;
	static uint32_t uiPassType = 0;
	static Stage eStage = STAGE_OPAQUE;
	static uint32_t uiDrawInPass = 0;
	static uint32_t uiLines = 0;

	// Bytecode hashes of the shaders seen, by device object.
	static std::unordered_map<const void*, uint32_t> kShaderHashes;

	// Returns the FNV-1a hash of the given bytes.
	static uint32_t Hash(const uint8_t* apData, size_t auiSize) {
		uint32_t uiHash = 2166136261u;

		for (size_t i = 0; i < auiSize; i++) {
			uiHash = (uiHash ^ apData[i]) * 16777619u;
		}

		return uiHash;
	}

	// Returns the bytecode hash of a vertex or pixel shader. Returns 0 for no shader.
	template <typename T>
	static uint32_t GetShaderHash(T* apShader) {
		if (!apShader) {
			return 0;
		}

		const auto it = kShaderHashes.find(apShader);

		if (it != kShaderHashes.end()) {
			return it->second;
		}

		UINT uiSize = 0;
		uint32_t uiHash = 0;

		if (SUCCEEDED(apShader->GetFunction(nullptr, &uiSize)) && uiSize) {
			std::vector<uint8_t> kFunction(uiSize);

			if (SUCCEEDED(apShader->GetFunction(kFunction.data(), &uiSize))) {
				uiHash = Hash(kFunction.data(), uiSize);
			}
		}

		kShaderHashes[apShader] = uiHash;
		return uiHash;
	}

	// Writes a short description of the texture at a sampler stage: size, format and mip count, or "-" if none.
	static void DescribeTexture(IDirect3DDevice9* apDevice, uint32_t auiStage, char* apBuffer, size_t auiSize) {
		IDirect3DBaseTexture9* pTexture = nullptr;
		apDevice->GetTexture(auiStage, &pTexture);

		if (!pTexture) {
			sprintf_s(apBuffer, auiSize, "-");
			return;
		}

		D3DSURFACE_DESC kDesc = {};

		if (pTexture->GetType() == D3DRTYPE_TEXTURE) {
			static_cast<IDirect3DTexture9*>(pTexture)->GetLevelDesc(0, &kDesc);
		}
		else if (pTexture->GetType() == D3DRTYPE_CUBETEXTURE) {
			static_cast<IDirect3DCubeTexture9*>(pTexture)->GetLevelDesc(0, &kDesc);
		}

		sprintf_s(apBuffer, auiSize, "%ux%u/%u/%u", kDesc.Width, kDesc.Height, kDesc.Format, pTexture->GetLevelCount());
		pTexture->Release();
	}

	// Starts a capture with the next main loop iteration.
	void Arm() {
		bArmed = true;
		_MESSAGE("Pass Log: Capturing the topper passes of the next frame.");
	}

	// Starts or ends the capture. Call once per main loop iteration, after the hotkeys.
	void OnMainLoop() {
		if (bCapturing) {
			_MESSAGE("Pass Log: Capture ended, %u draws logged.", uiLines);
		}

		bCapturing = bArmed;
		bArmed = false;
		uiLines = 0;
	}

	bool IsCapturing() {
		return bCapturing;
	}

	// Sets the topper pass that the following draws belong to.
	void BeginPass(const void* apRenderPass, uint32_t auiPassType, Stage aeStage) {
		pPass = static_cast<const BSShaderProperty::RenderPass*>(apRenderPass);
		uiPassType = auiPassType;
		eStage = aeStage;
		uiDrawInPass = 0;
	}

	void EndPass() {
		pPass = nullptr;
	}

	// Logs the device state of a draw inside a topper pass. Draws outside a topper pass are ignored.
	void OnDraw(IDirect3DDevice9* apDevice) {
		if (!bCapturing || !pPass) {
			return;
		}

		// Render States

		char cStates[512];
		size_t uiLength = 0;

		for (uint32_t i = 0; i < ARRAYSIZE(kStates); i++) {
			DWORD uiValue = 0;
			apDevice->GetRenderState(kStates[i], &uiValue);
			uiLength += sprintf_s(cStates + uiLength, sizeof(cStates) - uiLength, " %s=%u", STATE_NAMES[i], uiValue);
		}

		// Textures

		char cTextures[256];
		uiLength = 0;

		for (uint32_t i = 0; i < TEXTURE_STAGE_COUNT; i++) {
			char cTexture[48];
			DescribeTexture(apDevice, i, cTexture, sizeof(cTexture));
			uiLength += sprintf_s(cTextures + uiLength, sizeof(cTextures) - uiLength, " s%u=%s", i, cTexture);
		}

		// Shaders

		IDirect3DVertexShader9* pVertexShader = nullptr;
		IDirect3DPixelShader9* pPixelShader = nullptr;
		apDevice->GetVertexShader(&pVertexShader);
		apDevice->GetPixelShader(&pPixelShader);

		const uint32_t uiVertexHash = GetShaderHash(pVertexShader);
		const uint32_t uiPixelHash = GetShaderHash(pPixelShader);

		if (pVertexShader) {
			pVertexShader->Release();
		}

		if (pPixelShader) {
			pPixelShader->Release();
		}

		const NiGeometry* pGeometry = pPass->pGeometry;
		const char* pName = "";

		if (pGeometry && pGeometry->GetName()) {
			pName = pGeometry->GetName();
		}

		_MESSAGE("Pass Log: %s pass %u draw %u '%s' first %u lights %u vs %08X ps %08X%s%s",
			STAGE_NAMES[eStage], uiPassType, uiDrawInPass, pName, pPass->bIsFirst, pPass->ucNumLights, uiVertexHash, uiPixelHash, cStates, cTextures);

		// Pixel Shader Constants

		float fConstants[PIXEL_CONSTANT_COUNT][4];
		apDevice->GetPixelShaderConstantF(0, fConstants[0], PIXEL_CONSTANT_COUNT);

		char cConstants[PIXEL_CONSTANT_COUNT * 48];
		uiLength = 0;

		for (uint32_t i = 0; i < PIXEL_CONSTANT_COUNT; i++) {
			uiLength += sprintf_s(cConstants + uiLength, sizeof(cConstants) - uiLength, " c%u=%.3g,%.3g,%.3g,%.3g", i, fConstants[i][0], fConstants[i][1], fConstants[i][2], fConstants[i][3]);
		}

		_MESSAGE("Pass Log:   ps constants%s", cConstants);

		uiDrawInPass++;
		uiLines++;
	}

}
