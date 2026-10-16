#include "BlendToggle.hpp"

#include "Settings.hpp"

// Turns the terrain overlay and the topper crop on and off in game with `[Main] iToggleKey`.
// Toppers stay deferred and stencil tags are still written. Only the blending itself is skipped.

namespace BlendToggle {

	static bool bActive = true;
	static bool bKeyDown = false;

	// Returns whether the game window has the keyboard focus. Keys typed in other windows are ignored.
	static bool HasFocus() {
		DWORD uiProcess = 0;
		GetWindowThreadProcessId(GetForegroundWindow(), &uiProcess);
		return uiProcess == GetCurrentProcessId();
	}

	// Flips blending on and off when the toggle key goes down.
	// Call once per main loop iteration.
	void Poll() {
		if (Settings::uiToggleKey == 0)
			return;

		const bool bDown = HasFocus() && (GetAsyncKeyState(static_cast<int>(Settings::uiToggleKey)) & 0x8000) != 0;
		if (bDown && !bKeyDown) {
			bActive = !bActive;
			_MESSAGE("Toggle: Blending turned %s.", bActive ? "on" : "off");
		}
		bKeyDown = bDown;
	}

	bool IsActive() {
		return bActive;
	}

}
