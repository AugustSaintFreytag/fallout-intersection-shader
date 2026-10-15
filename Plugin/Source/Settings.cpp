#include "Settings.hpp"

#include <sstream>

namespace Settings {

	static constexpr const char* INI_PATH = "Data\\NVSE\\Plugins\\IntersectionBlend.ini";

	struct FormTypeName {
		const char*	pName;
		uint8_t		ucType;
	};

	// Base form types that can be selected for terrain blending.
	static constexpr FormTypeName kFormTypeNames[] = {
		{ "ACTI", 0x15 },
		{ "CONT", 0x1B },
		{ "DOOR", 0x1C },
		{ "STAT", 0x20 },
		{ "SCOL", 0x21 },
		{ "MSTT", 0x22 },
		{ "FURN", 0x27 },
	};

	static float GetFloat(const char* apSection, const char* apKey, float afDefault) {
		char cDefault[32];
		char cBuffer[32];
		sprintf_s(cDefault, "%f", afDefault);
		GetPrivateProfileString(apSection, apKey, cDefault, cBuffer, sizeof(cBuffer), INI_PATH);
		return static_cast<float>(atof(cBuffer));
	}

	static uint32_t GetUInt(const char* apSection, const char* apKey, uint32_t auiDefault) {
		return GetPrivateProfileInt(apSection, apKey, auiDefault, INI_PATH);
	}

	static std::string GetString(const char* apSection, const char* apKey, const char* apDefault) {
		char cBuffer[256];
		GetPrivateProfileString(apSection, apKey, apDefault, cBuffer, sizeof(cBuffer), INI_PATH);
		return cBuffer;
	}

	static std::string Trim(const std::string& arText) {
		const size_t uiStart = arText.find_first_not_of(" \t");
		if (uiStart == std::string::npos)
			return {};
		return arText.substr(uiStart, arText.find_last_not_of(" \t") - uiStart + 1);
	}

	static std::vector<uint8_t> ParseFormTypes(const std::string& arList) {
		std::vector<uint8_t> kTypes;
		std::stringstream kStream(arList);
		std::string sName;
		while (std::getline(kStream, sName, ',')) {
			sName = Trim(sName);
			if (sName.empty())
				continue;

			bool bFound = false;
			for (const FormTypeName& rType : kFormTypeNames) {
				if (!_stricmp(rType.pName, sName.c_str())) {
					kTypes.push_back(rType.ucType);
					bFound = true;
					break;
				}
			}
			if (!bFound)
				_MESSAGE("Settings: unknown form type %s, skipped", sName.c_str());
		}
		return kTypes;
	}

	// Reads an INI section with one base form per line, written as `Plugin.esm:FormID`.
	// Drops the load order byte of the form ID.
	static std::vector<FormEntry> GetFormList(const char* apSection) {
		std::vector<char> kBuffer(32767);
		const DWORD uiLength = GetPrivateProfileSection(apSection, kBuffer.data(), static_cast<DWORD>(kBuffer.size()), INI_PATH);

		std::vector<FormEntry> kEntries;
		for (const char* pLine = kBuffer.data(); pLine < kBuffer.data() + uiLength && *pLine; pLine += strlen(pLine) + 1) {
			std::string sLine(pLine);
			sLine = Trim(sLine.substr(0, sLine.find_first_of(";#=")));
			if (sLine.empty())
				continue;

			const size_t uiSeparator = sLine.rfind(':');
			char* pEnd = nullptr;
			const uint32_t uiFormID = uiSeparator != std::string::npos ? strtoul(sLine.c_str() + uiSeparator + 1, &pEnd, 16) : 0;
			if (uiSeparator == std::string::npos || uiSeparator == 0 || !pEnd || *pEnd) {
				_MESSAGE("Settings: [%s] entry \"%s\" is not Plugin.esm:FormID, skipped", apSection, sLine.c_str());
				continue;
			}

			kEntries.push_back({ Trim(sLine.substr(0, uiSeparator)), uiFormID & 0xFFFFFF });
		}
		return kEntries;
	}

	// Reads `IntersectionBlend.ini`. Missing keys keep their defaults.
	void Load() {
		bEnabled				= GetUInt("Main", "bEnabled", bEnabled) != 0;
		bStencilTags			= GetUInt("Main", "bStencilTags", bStencilTags) != 0;
		bLipping				= GetUInt("Blend", "bLipping", bLipping) != 0;
		bUnrolledMapping		= GetUInt("Blend", "bUnrolledMapping", bUnrolledMapping) != 0;
		bGroundShading			= GetUInt("Blend", "bGroundShading", bGroundShading) != 0;
		fMaxDistance			= GetFloat("Blend", "fMaxDistance", fMaxDistance);
		fBlendRange				= GetFloat("Blend", "fBlendRange", fBlendRange);
		fMaxRayGap				= GetFloat("Blend", "fMaxRayGap", fMaxRayGap);
		kTerrainBlendTypes		= ParseFormTypes(GetString("Selection", "sTerrainBlendTypes", "STAT"));
		kTerrainBlendBlacklist	= GetFormList("TerrainBlendBlacklist");
		eDebugMode				= static_cast<DebugMode>(GetUInt("Debug", "iDebugMode", eDebugMode));
		bProfile				= GetUInt("Debug", "bProfile", bProfile) != 0;

		_MESSAGE("Settings: stencil tags %u, lipping %u, unrolled mapping %u, ground shading %u", bStencilTags, bLipping, bUnrolledMapping, bGroundShading);
		_MESSAGE("Settings: enabled %u, max distance %.1f, blend range %.1f, max ray gap %.1f, %u terrain blend types, %u blacklist entries, debug mode %u",
			bEnabled, fMaxDistance, fBlendRange, fMaxRayGap, static_cast<uint32_t>(kTerrainBlendTypes.size()), static_cast<uint32_t>(kTerrainBlendBlacklist.size()), eDebugMode);
	}

}
