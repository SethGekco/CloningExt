#include "CloningExt.h"

#include <Phobos.h>
#include <Syringe.h>
#include <Utilities/Patch.h>
#include <Utilities/Debug.h>
#include <Utilities/Macro.h>

HANDLE CloningExtDLL::hInstance = nullptr;
bool CloningExtDLL::AresLineageLoaded = false;

char CloningExtDLL::readBuffer[CloningExtDLL::readLength];
wchar_t CloningExtDLL::wideBuffer[CloningExtDLL::readLength];

void CloningExtDLL::ExeRun()
{
	Patch::ApplyStatic();

	// Antares OWNS the cloning system (KickOutClones + the four dispatch hooks
	// at 0x444DBC / 0x4445F6 / 0x44441A / 0x4449DF). Everything this DLL adds is
	// layered on top of that. Without an Ares-lineage DLL there is no base
	// cloning to augment: CloneCount extras, prereq/house slots and the veterancy
	// controls all become no-ops. The un-contended CloneInitialStrength hook at
	// 0x443C81 still works standalone, because vanilla CloningVats reach it too.
	if (GetModuleHandleA("Antares.dll"))
	{
		CloningExtDLL::AresLineageLoaded = true;
		Debug::Log("[CloningExt] Antares detected. Augmenting its cloning system.\n");
	}
	else if (GetModuleHandleA("Ares.dll"))
	{
		CloningExtDLL::AresLineageLoaded = true;
		Debug::Log("[CloningExt] Ares detected. Prefer Antares, but augmenting anyway.\n");
	}
	else
	{
		Debug::Log("[CloningExt] *** WARNING: no Antares/Ares DLL loaded. Only the "
		           "CloneInitialStrength layer (vanilla CloningVats) will function; "
		           "CloneCount, prereq/house clone slots and clone-veterancy controls "
		           "require Antares.\n");
	}
}

bool __stdcall DllMain(HANDLE hInstance, DWORD dwReason, LPVOID)
{
	if (dwReason == DLL_PROCESS_ATTACH)
	{
		CloningExtDLL::hInstance = hInstance;
		Phobos::hInstance = hInstance; // needed by Patch::ApplyStatic
	}
	return true;
}

SYRINGE_HANDSHAKE(pInfo)
{
	pInfo->Message = const_cast<char*>("CloningExt");
	return S_OK;
}

// Main-loop entry, so static patches apply at the right time.
DEFINE_HOOK(0x7CD810, CloningExt_ExeRun, 0x9)
{
	CloningExtDLL::ExeRun();
	return 0;
}

// Flush the deferred debug log once the command line has been parsed.
DEFINE_HOOK(0x52F639, CloningExt_CmdLineParse, 0x5)
{
	Debug::LogDeferredFinalize();
	return 0;
}
