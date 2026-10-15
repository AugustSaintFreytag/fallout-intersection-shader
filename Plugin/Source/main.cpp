#include "Bethesda/BSMemory.hpp"
#include "nvse/PluginAPI.h"

#include "Dependencies.hpp"
#include "OverlayPass.hpp"
#include "RenderStates.hpp"
#include "Settings.hpp"
#include "StencilTags.hpp"
#include "TerrainPasses.hpp"

BS_ALLOCATORS

IDebugLog gLog("logs\\IntersectionBlend.log");

static constexpr const char*	PLUGIN_NAME		= "Intersection Blend";
static constexpr uint32_t		PLUGIN_VERSION	= 1;

static NVSEMessagingInterface*	pMsgInterface	= nullptr;
static PluginHandle				uiPluginHandle	= 0;

static void MessageHandler(NVSEMessagingInterface::Message* apMessage) {
	switch (apMessage->type) {
		case NVSEMessagingInterface::kMessage_DeferredInit:
			// Runs after every plugin has loaded. The render state hook then chains to the hook of LOD Flicker Fix.
			RenderStates::InstallHook();
			StencilTags::LoadSelection();
			OverlayPass::Initialize();
			break;
		case NVSEMessagingInterface::kMessage_MainGameLoop:
			// Runs before the frame is rendered. Drops passes of a frame that got no depth callback.
			OverlayPass::OnMainLoop();
			TerrainPasses::Clear();
			break;
	}
}

EXTERN_DLL_EXPORT bool NVSEPlugin_Query(const NVSEInterface* nvse, PluginInfo* info) {
	info->infoVersion	= PluginInfo::kInfoVersion;
	info->name			= PLUGIN_NAME;
	info->version		= PLUGIN_VERSION;

	return !nvse->isEditor;
}

EXTERN_DLL_EXPORT bool NVSEPlugin_Load(NVSEInterface* nvse) {
	_MESSAGE("%s %u", PLUGIN_NAME, PLUGIN_VERSION);

	if (!Dependencies::Check(nvse))
		return false;

	Settings::Load();

	pMsgInterface = static_cast<NVSEMessagingInterface*>(nvse->QueryInterface(kInterface_Messaging));
	uiPluginHandle = nvse->GetPluginHandle();
	pMsgInterface->RegisterListener(uiPluginHandle, "NVSE", MessageHandler);

	TerrainPasses::InstallHooks();
	StencilTags::InstallHooks();
	Dependencies::RegisterPostResolveDepthCallback(OverlayPass::OnPostResolveDepth);

	return true;
}

BOOL WINAPI DllMain(HANDLE hDllHandle, DWORD dwReason, LPVOID lpreserved) {
	if (dwReason == DLL_PROCESS_ATTACH)
		DisableThreadLibraryCalls(static_cast<HMODULE>(hDllHandle));
	return TRUE;
}
