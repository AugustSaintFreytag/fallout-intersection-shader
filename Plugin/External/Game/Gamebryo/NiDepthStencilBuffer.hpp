#pragma once

#include "Ni2DBuffer.hpp"

NiSmartPointer(NiDepthStencilBuffer);

class NiDepthStencilBuffer : public Ni2DBuffer {
public:
	NIRTTI_ADDRESS(0x11F603C);
};

ASSERT_SIZE(NiDepthStencilBuffer, 0x014);