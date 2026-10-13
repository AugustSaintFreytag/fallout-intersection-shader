#include "NiAVObject.hpp"

void NiAVObject::SetBit(uint32_t auData, bool abVal) {
	m_uiFlags.SetBit(auData, abVal);
}

bool NiAVObject::GetBit(uint32_t auData) const {
	return m_uiFlags.IsSet(auData);
}
