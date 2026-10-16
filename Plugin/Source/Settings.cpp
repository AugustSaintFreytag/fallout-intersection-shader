#include "Settings.hpp"

#include <algorithm>
#include <sstream>

namespace Settings {

	static constexpr const char* INI_PATH		= "Data\\NVSE\\Plugins\\IntersectionBlend.ini";
	static constexpr const char* FORMS_INI_PATH	= "Data\\NVSE\\Plugins\\IntersectionBlendForms.ini";

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

	// Reads a section of `IntersectionBlendForms.ini` with one editor ID per line.
	static std::vector<std::string> GetEditorIDList(const char* apSection) {
		std::vector<char> kBuffer(32767);
		const DWORD uiLength = GetPrivateProfileSection(apSection, kBuffer.data(), static_cast<DWORD>(kBuffer.size()), FORMS_INI_PATH);

		std::vector<std::string> kEntries;
		for (const char* pLine = kBuffer.data(); pLine < kBuffer.data() + uiLength && *pLine; pLine += strlen(pLine) + 1) {
			std::string sLine(pLine);
			sLine = Trim(sLine.substr(0, sLine.find_first_of(";#=")));
			if (!sLine.empty())
				kEntries.push_back(sLine);
		}
		return kEntries;
	}

	// Reads `IntersectionBlend.ini` and the form lists in `IntersectionBlendForms.ini`. Missing keys keep their defaults.
	void Load() {
		bEnabled				= GetUInt("Main", "bEnabled", bEnabled) != 0;
		bUnrolledMapping		= GetUInt("Blend", "bUnrolledMapping", bUnrolledMapping) != 0;
		fMaxDistance			= GetFloat("Blend", "fMaxDistance", fMaxDistance);
		fBlendRange				= GetFloat("Blend", "fBlendRange", fBlendRange);
		fCropHeight				= GetFloat("StaticBlend", "fCropHeight", fCropHeight);
		fCropScale				= std::max(GetFloat("StaticBlend", "fCropScale", fCropScale), 1.0f);
		kTerrainBlendTypes		= ParseFormTypes(GetString("Selection", "sTerrainBlendTypes", "STAT, SCOL"));
		kTerrainToStaticBlacklist	= GetEditorIDList("TerrainToStaticBlacklist");
		kStaticToStaticWhitelist	= GetEditorIDList("StaticToStaticWhitelist");
		eDebugMode				= static_cast<DebugMode>(GetUInt("Debug", "iDebugMode", eDebugMode));
		bProfile				= GetUInt("Debug", "bProfile", bProfile) != 0;
		bDeferToppers			= GetUInt("Debug", "bDeferToppers", bDeferToppers) != 0;

		_MESSAGE("Settings: enabled %u, unrolled mapping %u, max distance %.1f, blend range %.1f, %u terrain blend types, %u blacklist entries, %u toppers, debug mode %u, profile %u",
			bEnabled, bUnrolledMapping, fMaxDistance, fBlendRange, static_cast<uint32_t>(kTerrainBlendTypes.size()), static_cast<uint32_t>(kTerrainToStaticBlacklist.size()),
			static_cast<uint32_t>(kStaticToStaticWhitelist.size()), eDebugMode, bProfile);
	}

}
