#include "BSShaderAccumulator.hpp"

// GAME - 0xB63120
// GECK - 0x902D60
bool BSShaderAccumulator::GetWaterPassesWithinRange(BSShaderManager::RenderPassType aeStartRange, BSShaderManager::RenderPassType aeEndRange) {
#if GAME
	return ThisCall<bool>(0xB63120, this, aeStartRange, aeEndRange);
#else
	return ThisCall<bool>(0x902D60, this, aeStartRange, aeEndRange);
#endif
}

// GAME - 0xB63770
// GECK - 0x9031B0
void BSShaderAccumulator::RenderBatches(BSShaderManager::RenderPassType aeStart, BSShaderManager::RenderPassType aeEnd, bool abAlphaPass, BSBatchRenderer::GroupType aeGeometryGroup) {
#if GAME
	ThisCall(0xB63770, this, aeStart, aeEnd, abAlphaPass, aeGeometryGroup);
#else
	ThisCall(0x9031B0, this, aeStart, aeEnd, abAlphaPass, aeGeometryGroup);
#endif
}

// GAME - 0xB639E0
// GECK - 0x903420
void BSShaderAccumulator::RenderGeometryGroup(BSBatchRenderer::GroupType aeGeometryGroup, bool abAlphaPass) {
#if GAME
	ThisCall(0xB639E0, this, aeGeometryGroup, abAlphaPass);
#else
	ThisCall(0x903420, this, aeGeometryGroup, abAlphaPass);
#endif
}

// GAME - 0xB650C0
// GECK - 0x9042A0
void BSShaderAccumulator::RenderAlphaGeometry(BSBatchRenderer::AlphaGroupType aeGeometryGroup) {
#if GAME
	ThisCall(0xB650C0, this, aeGeometryGroup);
#else
	ThisCall(0x9042A0, this, aeGeometryGroup);
#endif
}
