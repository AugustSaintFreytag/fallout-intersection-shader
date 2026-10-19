#include "Hotkeys.hpp"

#include "PassLog.hpp"
#include "Settings.hpp"

// In-game keys for testing, read with `GetAsyncKeyState` while the game window has focus.
// The INI gives keys as DirectInput scancodes, as in the GECK and NVSE `IsKeyPressed`.
// `[Main] iToggleKey` turns the terrain overlay and the topper blend on and off. Toppers stay deferred and stencil tags
// are still written. Only the blending itself is skipped.
// `[Main] iReloadKey` reads the live settings from `IntersectionBlend.ini` again (`Settings::LoadLive`).

namespace Hotkeys {

	// Returns the Windows virtual-key code of a DirectInput scancode.
	// Returns 0 if the scancode has no virtual-key code.
	static uint32_t GetVirtualKey(uint32_t auiScancode) {
		uint32_t uiScancode = auiScancode;

		// DirectInput sets bit 7 for keys sent with the E0 prefix, e.g. arrow keys and right Ctrl.
		// `MapVirtualKey` takes these keys with the prefix in the high byte.
		if (auiScancode & 0x80) {
			uiScancode = 0xE000 | (auiScancode & 0x7F);
		}

		return MapVirtualKeyA(uiScancode, MAPVK_VSC_TO_VK_EX);
	}

	// A key that acts once when it goes down.
	struct Key {
		bool bDown = false;

		// Returns whether the key with the given DirectInput scancode went down since the last call.
		bool Pressed(uint32_t auiScancode, bool abFocus) {
			const bool bWasDown = bDown;
			bDown = false;

			if (auiScancode != 0 && abFocus) {
				const uint32_t uiVirtualKey = GetVirtualKey(auiScancode);

				if (uiVirtualKey != 0) {
					bDown = (GetAsyncKeyState(static_cast<int>(uiVirtualKey)) & 0x8000) != 0;
				}
			}

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

			if (Settings::bLogTopperPasses) {
				PassLog::Arm();
			}
		}
	}

	bool IsBlendActive() {
		return bBlendActive;
	}

}
