#include "Settings.hpp"

namespace Settings {
	static constexpr const char* INI_PATH = "Data\\NVSE\\Plugins\\IntersectionBlend.ini";

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

	void Load() {
		bEnabled		= GetUInt("Main", "bEnabled", bEnabled) != 0;
		fMaxDistance	= GetFloat("Blend", "fMaxDistance", fMaxDistance);
		fBlendRange		= GetFloat("Blend", "fBlendRange", fBlendRange);
		fMaxRayGap		= GetFloat("Blend", "fMaxRayGap", fMaxRayGap);
		eDebugMode		= static_cast<DebugMode>(GetUInt("Debug", "iDebugMode", eDebugMode));

		_MESSAGE("Settings: enabled %u, max distance %.1f, blend range %.1f, max ray gap %.1f, debug mode %u", bEnabled, fMaxDistance, fBlendRange, fMaxRayGap, eDebugMode);
	}
}
