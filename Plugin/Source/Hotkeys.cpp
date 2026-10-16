#include "Hotkeys.hpp"

#include "Settings.hpp"

// In-game keys for testing, read with `GetAsyncKeyState` while the game window has focus.
// `[Main] iToggleKey` turns the terrain overlay and the topper crop on and off. Toppers stay deferred and stencil tags
// are still written. Only the blending itself is skipped.
// `[Main] iReloadKey` reads the live settings from `IntersectionBlend.ini` again (`Settings::LoadLive`).

namespace Hotkeys {

	// A key that acts once when it goes down.
	struct Key {
		bool bDown = false;

		// Returns whether the key went down since the last call.
		bool Pressed(uint32_t auiCode, bool abFocus) {
			const bool bWasDown = bDown;
			bDown = auiCode != 0 && abFocus && (GetAsyncKeyState(static_cast<int>(auiCode)) & 0x8000) != 0;
			return bDown && !bWasDown;
		}
	};

	static bool bBlendActive = true;
	static Key kToggleKey;
	static Key kReloadKey;

	// Returns whether the game window has the keyboard focus. Keys typed in other windows are ignored.
	static bool HasFocus() {
		DWORD uiProcess = 0;
		GetWindowThreadProcessId(GetForegroundWindow(), &uiProcess);
		return uiProcess == GetCurrentProcessId();
	}

	// Acts on keys that went down since the last call. Call once per main loop iteration.
	void Poll() {
		const bool bFocus = HasFocus();

		if (kToggleKey.Pressed(Settings::uiToggleKey, bFocus)) {
			bBlendActive = !bBlendActive;
			_MESSAGE("Hotkeys: Blending turned %s.", bBlendActive ? "on" : "off");
		}

		if (kReloadKey.Pressed(Settings::uiReloadKey, bFocus)) {
			_MESSAGE("Hotkeys: Reloading live settings.");
			Settings::LoadLive();
		}
	}

	bool IsBlendActive() {
		return bBlendActive;
	}

}
