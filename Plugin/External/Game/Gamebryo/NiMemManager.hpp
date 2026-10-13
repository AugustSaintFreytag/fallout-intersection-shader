#pragma once

#include "NiAllocator.hpp"

class NiMemManager {
public:
	NiAllocator* m_pkAllocator;

	static NiMemManager* GetSingleton();
};

ASSERT_SIZE(NiMemManager, 0x4);