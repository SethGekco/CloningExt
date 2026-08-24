#pragma once

#include <Windows.h>

class CloningExtDLL
{
public:
	static HANDLE hInstance;

	static constexpr size_t readLength = 2048;
	static char readBuffer[readLength];
	static wchar_t wideBuffer[readLength];

	// True once we have confirmed an Ares-lineage DLL (Antares/Ares) is loaded.
	// Cloning dispatch itself is owned by Antares, so our augmentation hooks are
	// only meaningful when it is present. Recorded at ExeRun and logged.
	static bool AresLineageLoaded;

	static void ExeRun();
};
