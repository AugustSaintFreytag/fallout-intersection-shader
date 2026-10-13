#include "NiRefObject.hpp"

// GAME - 0x4968B0
NiRefObject::NiRefObject() {
	m_uiRefCount = 0;
	InterlockedIncrement(&ms_uiObjects.Get());
}

// GAME - 0xA5D3D0
NiRefObject::~NiRefObject() {
	InterlockedDecrement(&ms_uiObjects.Get());
}

// GAME - 0xCFCC20
void NiRefObject::DeleteThis() {
	delete this;
}