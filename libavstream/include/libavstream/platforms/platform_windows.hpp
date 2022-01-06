// libavstream
// (c) Copyright 2018-2022 Simul Software Ltd

#pragma once

#include <libavstream/common.hpp>
#include <libavstream/platforms/platform_common.hpp>

#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

namespace avs
{

typedef HMODULE LibraryHandle;
typedef FARPROC ProcAddress;
typedef LARGE_INTEGER Timestamp;

class PlatformWindows
{
	static LARGE_INTEGER frequency ;
public:
	PlatformWindows()
	{
	}
	static LibraryHandle openLibrary(const char* filename);
	static bool closeLibrary(LibraryHandle hLibrary);
	static ProcAddress getProcAddress(LibraryHandle hLibrary, const char* function);

	static Timestamp getTimestamp();
	static double getTimeElapsed(const Timestamp& tBegin, const Timestamp& tEnd);
	static double getTimeElapsedInSeconds(const Timestamp& tBegin, const Timestamp& tEnd);

	static SystemTime getSystemTime();
};

using Platform = PlatformWindows;

} // avs
