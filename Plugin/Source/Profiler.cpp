#include "Profiler.hpp"

#include "Settings.hpp"

#include "Gamebryo/NiDX9Renderer.hpp"

#include <algorithm>

// GPU timing of the post-resolve callback, logged with `[Debug] bProfile`.
//
// Every frame issues one timestamp query before the first stage and one after each stage, inside a disjoint query.
// Results are read a few frames later from a ring of query sets, so reading them never waits for the GPU.
// Frames whose disjoint query reports a change of the timestamp frequency are dropped.
// The log gets one line per stage every few seconds: average and maximum in milliseconds.
namespace Profiler {

	static constexpr uint32_t	SET_COUNT			= 4;	// Frames in flight before a set is read again
	static constexpr double		LOG_INTERVAL		= 5.0;	// Seconds
	static constexpr const char*	STAGE_NAMES[STAGE_COUNT] = { "toppers", "field", "ground", "overlay" };

	// The queries of one frame.
	struct QuerySet {
		IDirect3DQuery9*	pDisjoint						= nullptr;
		IDirect3DQuery9*	pFrequency						= nullptr;
		IDirect3DQuery9*	pTimestamps[STAGE_COUNT + 1]	= {};
		bool				bPending						= false;
	};

	// Sum and maximum of one measured value over the log interval.
	struct Accumulator {
		double		dSum	= 0.0;
		double		dMax	= 0.0;

		void Add(double adValue) {
			dSum += adValue;
			dMax = std::max(dMax, adValue);
		}
	};

	static IDirect3DDevice9*	pDevice					= nullptr;
	static QuerySet				kSets[SET_COUNT];
	static uint32_t				uiCurrentSet			= 0;
	static uint32_t				uiNextTimestamp			= 0;	// In the current set
	static bool					bInFrame				= false;
	static bool					bReady					= false;

	// Log Interval
	static Accumulator			kStageTimes[STAGE_COUNT];
	static Accumulator			kReplayCpu;
	static Accumulator			kPasses;
	static Accumulator			kDrawn;
	static Accumulator			kSkipped;
	static uint32_t				uiForeignDraws			= 0;
	static Accumulator			kScreens;
	static Accumulator			kTagged;
	static Accumulator			kToppers;
	static Accumulator			kDistant;
	static uint32_t				uiTagFrames				= 0;
	static uint32_t				uiGpuFrames				= 0;
	static uint32_t				uiReplayFrames			= 0;
	static uint32_t				uiDroppedFrames			= 0;
	static LARGE_INTEGER		kIntervalStart			= {};
	static LARGE_INTEGER		kCounterFrequency		= {};

	template <typename T>
	static void SafeRelease(T*& apObject) {
		if (apObject) {
			apObject->Release();
			apObject = nullptr;
		}
	}

	static void ReleaseQueries() {
		for (QuerySet& rSet : kSets) {
			SafeRelease(rSet.pDisjoint);
			SafeRelease(rSet.pFrequency);
			for (IDirect3DQuery9*& rpTimestamp : rSet.pTimestamps)
				SafeRelease(rpTimestamp);
			rSet.bPending = false;
		}
		bReady = false;
		bInFrame = false;
	}

	static bool __cdecl OnDeviceReset(bool abBeforeReset, void*) {
		if (abBeforeReset)
			ReleaseQueries();
		return true;
	}

	static bool CreateQueries() {
		for (QuerySet& rSet : kSets) {
			bool bCreated = SUCCEEDED(pDevice->CreateQuery(D3DQUERYTYPE_TIMESTAMPDISJOINT, &rSet.pDisjoint))
				&& SUCCEEDED(pDevice->CreateQuery(D3DQUERYTYPE_TIMESTAMPFREQ, &rSet.pFrequency));
			for (IDirect3DQuery9*& rpTimestamp : rSet.pTimestamps)
				bCreated = bCreated && SUCCEEDED(pDevice->CreateQuery(D3DQUERYTYPE_TIMESTAMP, &rpTimestamp));
			if (!bCreated) {
				ReleaseQueries();
				return false;
			}
		}
		bReady = true;
		return true;
	}

	// Registers for device resets if profiling is on. The queries are created on the first frame.
	// Call once the D3D device exists, at NVSE DeferredInit.
	void Initialize() {
		if (!Settings::bProfile)
			return;

		NiDX9Renderer* pRenderer = NiDX9Renderer::GetSingleton();
		pDevice = pRenderer->GetD3DDevice();
		pRenderer->AddResetNotificationFunc(OnDeviceReset, nullptr);
		QueryPerformanceFrequency(&kCounterFrequency);
		QueryPerformanceCounter(&kIntervalStart);
		_MESSAGE("Profiler enabled, logging every %.0f s", LOG_INTERVAL);
	}

	// Reads the results of a set issued a few frames ago, if the GPU has them. Returns false if not yet available.
	static bool ReadSet(QuerySet& arSet) {
		BOOL bDisjoint = FALSE;
		UINT64 uiFrequency = 0;
		UINT64 uiTimestamps[STAGE_COUNT + 1] = {};

		if (arSet.pDisjoint->GetData(&bDisjoint, sizeof(bDisjoint), 0) != S_OK
			|| arSet.pFrequency->GetData(&uiFrequency, sizeof(uiFrequency), 0) != S_OK)
			return false;
		for (uint32_t i = 0; i <= STAGE_COUNT; i++) {
			if (arSet.pTimestamps[i]->GetData(&uiTimestamps[i], sizeof(UINT64), 0) != S_OK)
				return false;
		}

		arSet.bPending = false;
		if (bDisjoint || uiFrequency == 0) {
			uiDroppedFrames++;
			return true;
		}

		for (uint32_t i = 0; i < STAGE_COUNT; i++)
			kStageTimes[i].Add(static_cast<double>(uiTimestamps[i + 1] - uiTimestamps[i]) * 1000.0 / static_cast<double>(uiFrequency));
		uiGpuFrames++;
		return true;
	}

	// Logs the averages and maxima of the interval and starts a new one, once the interval has passed.
	static void LogInterval() {
		LARGE_INTEGER kNow;
		QueryPerformanceCounter(&kNow);
		const double dElapsed = static_cast<double>(kNow.QuadPart - kIntervalStart.QuadPart) / static_cast<double>(kCounterFrequency.QuadPart);
		if (dElapsed < LOG_INTERVAL)
			return;

		if (uiGpuFrames) {
			char cLine[512];
			int iLength = sprintf_s(cLine, "Profile, %u frames (%u dropped), GPU ms avg/max:", uiGpuFrames, uiDroppedFrames);
			double dTotal = 0.0;
			for (uint32_t i = 0; i < STAGE_COUNT; i++) {
				const double dAverage = kStageTimes[i].dSum / uiGpuFrames;
				dTotal += dAverage;
				iLength += sprintf_s(cLine + iLength, sizeof(cLine) - iLength, " %s %.2f/%.2f,", STAGE_NAMES[i], dAverage, kStageTimes[i].dMax);
			}
			sprintf_s(cLine + iLength, sizeof(cLine) - iLength, " total %.2f", dTotal);
			_MESSAGE("%s", cLine);
		}
		if (uiReplayFrames) {
			_MESSAGE("Profile, replay: CPU ms avg/max %.2f/%.2f, passes avg/max %.1f/%.0f, triangles avg/max %.1f/%.0f, skipped draws avg/max %.1f/%.0f, %u skipped with another vertex buffer, scissored screens avg/max %.2f/%.2f",
				kReplayCpu.dSum / uiReplayFrames, kReplayCpu.dMax, kPasses.dSum / uiReplayFrames, kPasses.dMax,
				kDrawn.dSum / uiReplayFrames, kDrawn.dMax, kSkipped.dSum / uiReplayFrames, kSkipped.dMax, uiForeignDraws,
				kScreens.dSum / uiReplayFrames, kScreens.dMax);
		}
		if (uiTagFrames) {
			_MESSAGE("Profile, tags: tagged geometry avg/max %.1f/%.0f, of it toppers avg/max %.1f/%.0f, beyond max distance avg/max %.1f/%.0f",
				kTagged.dSum / uiTagFrames, kTagged.dMax, kToppers.dSum / uiTagFrames, kToppers.dMax, kDistant.dSum / uiTagFrames, kDistant.dMax);
		}

		for (Accumulator& rTime : kStageTimes)
			rTime = {};
		kReplayCpu = {};
		kPasses = {};
		kDrawn = {};
		kSkipped = {};
		uiForeignDraws = 0;
		kScreens = {};
		kTagged = {};
		kToppers = {};
		kDistant = {};
		uiTagFrames = 0;
		uiGpuFrames = 0;
		uiReplayFrames = 0;
		uiDroppedFrames = 0;
		kIntervalStart = kNow;
	}

	// Starts timing a post-resolve callback. Reads finished sets first and logs the interval when it is due.
	void BeginFrame(IDirect3DDevice9* apDevice) {
		if (!Settings::bProfile || apDevice != pDevice)
			return;
		if (!bReady && !CreateQueries()) {
			_MESSAGE("Profiler: timestamp queries not supported, profiling disabled");
			Settings::bProfile = false;
			return;
		}

		for (QuerySet& rSet : kSets) {
			if (rSet.bPending)
				ReadSet(rSet);
		}
		LogInterval();

		// A set still in flight is dropped: the GPU is more than `SET_COUNT` frames behind.
		uiCurrentSet = (uiCurrentSet + 1) % SET_COUNT;
		QuerySet& rSet = kSets[uiCurrentSet];
		if (rSet.bPending) {
			rSet.bPending = false;
			uiDroppedFrames++;
		}

		rSet.pDisjoint->Issue(D3DISSUE_BEGIN);
		rSet.pFrequency->Issue(D3DISSUE_END);
		rSet.pTimestamps[0]->Issue(D3DISSUE_END);
		uiNextTimestamp = 1;
		bInFrame = true;
	}

	// Marks the end of a stage. Stages before it that did not run this frame end at the same time, with 0 ms.
	void EndStage(Stage aeStage) {
		if (!bInFrame)
			return;

		QuerySet& rSet = kSets[uiCurrentSet];
		while (uiNextTimestamp <= static_cast<uint32_t>(aeStage) + 1)
			rSet.pTimestamps[uiNextTimestamp++]->Issue(D3DISSUE_END);
	}

	// Ends timing a post-resolve callback. Stages that did not run end here.
	void EndFrame() {
		if (!bInFrame)
			return;

		EndStage(static_cast<Stage>(STAGE_COUNT - 1));
		QuerySet& rSet = kSets[uiCurrentSet];
		rSet.pDisjoint->Issue(D3DISSUE_END);
		rSet.bPending = true;
		bInFrame = false;
	}

	// Records the work of one overlay replay: passes, full-screen triangles drawn, draws skipped, skipped draws with
	// another vertex buffer, the summed scissor area in screens, and the CPU time spent issuing them.
	void RecordReplay(uint32_t auiPasses, uint32_t auiDrawn, uint32_t auiSkipped, uint32_t auiForeign, double adScreens, double adCpuMilliseconds) {
		if (!Settings::bProfile)
			return;

		kPasses.Add(auiPasses);
		kDrawn.Add(auiDrawn);
		kSkipped.Add(auiSkipped);
		uiForeignDraws += auiForeign;
		kScreens.Add(adScreens);
		kReplayCpu.Add(adCpuMilliseconds);
		uiReplayFrames++;
	}

	// Records the eligible geometry of one tagged opaque stage: tagged, tagged as toppers, and left untagged beyond
	// `fMaxDistance`.
	void RecordTags(uint32_t auiTagged, uint32_t auiToppers, uint32_t auiDistant) {
		if (!Settings::bProfile)
			return;

		kTagged.Add(auiTagged);
		kToppers.Add(auiToppers);
		kDistant.Add(auiDistant);
		uiTagFrames++;
	}

}
