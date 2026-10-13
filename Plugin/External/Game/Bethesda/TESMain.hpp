#pragma once

#include "BSShaderAccumulator.hpp"
#include "Gamebryo/NiCamera.hpp"
#include "Gamebryo/NiDX9Renderer.hpp"
#include "SceneGraph.hpp"

class Sun;
class BSAudioManager;
class BSPackedTaskQueue;
class ImageSpaceManager;
class BSClearZNode;
class VATS;

// Officially just "Main"	
class TESMain {
public:
	bool					bOneMore;
	bool					bQuitGame;
	bool					bResetGame;
	bool					bGameActive;
	bool					bOnIdle;
	bool					unk05;
	bool					bIsFlyCam;
	bool					bFreezeTime;
	HWND					hWnd;
	HINSTANCE				hInstance;
	uint32_t				uiMainThreadID;
	HANDLE					hMainThread;
	uint32_t				pad0[4];
	uint32_t				kTaskQueue[10];  // BSPackedTaskQueue
	uint32_t				pad1[4];
	uint32_t				kSecondaryTaskQueue[10];  // BSPackedTaskQueue
	BSShaderAccumulatorPtr	spWorldAccum;
	BSShaderAccumulatorPtr	sp1stPersonAccum;
	BSShaderAccumulatorPtr	spAimDOFAccumulator;
	BSShaderAccumulatorPtr	spScreenSplatterAccum;
	BSShaderAccumulatorPtr	sp3DMenuAccumulator;
	bool					bShouldSetViewPort;
	NiCameraPtr				spFirstPersonCamera;

	static constexpr AddressPtr<NiDX9RendererPtr, 0x11C73B4> spRenderer;
	static constexpr AddressPtr<float, 0x11C3C08> fCellAppTime;
	static constexpr AddressPtr<float, 0x11DEA30> fFrameAnimTime;

	static constexpr AddressPtr<bool, 0x11DE9D1> bOffsetViewModelLights;

	static TESMain* GetSingleton();
	static SceneGraph* GetWorldSceneGraph();
};

ASSERT_SIZE(TESMain, 0xA4);