#include "NiObjectNET.hpp"

// GAME - 0x4AD1B0
NiObjectNET::CopyType NiObjectNET::GetDefaultCopyType() {
	return *reinterpret_cast<CopyType*>(0x11F4300);
}

// GAME - 0x4AD1C0
char NiObjectNET::GetDefaultAppendCharacter() {
	return *reinterpret_cast<char*>(0x11A94A8);
}
