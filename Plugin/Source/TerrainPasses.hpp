#pragma once

#include <functional>

class BSRenderedTexture;
class NiGeometry;
class NiPoint3;

namespace TerrainPasses {

	void InstallHooks();

	void Clear();

	// A filter that decides per recorded pass whether to draw it, by its landscape geometry.
	typedef std::function<bool(const NiGeometry* apGeometry)> PassFilter;

	// A callback run right before a replayed pass is drawn, with its landscape geometry.
	typedef std::function<void(const NiGeometry* apGeometry)> PassCallback;

	void SortNearestFirst(const NiPoint3& arPosition);

	uint32_t Replay(const BSRenderedTexture* apTarget, const PassFilter& arFilter = nullptr, const PassCallback& arBeforeDraw = nullptr);

	void LogRecorded(const BSRenderedTexture* apTarget);

	bool Any(const BSRenderedTexture* apTarget, const PassFilter& arFilter);

}
