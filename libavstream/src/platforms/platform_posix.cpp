// libavstream
// (c) Copyright 2018 Simul.co

#include <dlfcn.h>
#include <libavstream/platforms/platform_posix.hpp>

namespace avs {
	
LibraryHandle PlatformPOSIX::openLibrary(const char* filename)
{
	return dlopen(filename, RTLD_LAZY);
}

bool PlatformPOSIX::closeLibrary(LibraryHandle hLibrary)
{
	return dlclose(hLibrary) == 0;
}
	
ProcAddress PlatformPOSIX::getProcAddress(LibraryHandle hLibrary, const char* function)
{
	return dlsym(hLibrary, function);
}
	
Timestamp PlatformPOSIX::getTimestamp()
{
	struct timespec timestamp;
	clock_gettime(CLOCK_MONOTONIC_RAW, &timestamp);
	return timestamp;
}
	
double PlatformPOSIX::getTimeElapsedInMilliseconds(const Timestamp& tBegin, const Timestamp& tEnd)
{
	const long double dt_sec  = tEnd.tv_sec  - tBegin.tv_sec;
	const long double dt_nsec = tEnd.tv_nsec - tBegin.tv_nsec;
	// Convert to milliseconds
	return dt_sec * 1000.0 + dt_nsec / 1000000.0;
}

double PlatformPOSIX::getTimeElapsedInSeconds(const Timestamp& tBegin, const Timestamp& tEnd)
{
	const long double dt_sec  = tEnd.tv_sec  - tBegin.tv_sec;
	const long double dt_nsec = tEnd.tv_nsec - tBegin.tv_nsec;
	return dt_sec + dt_nsec / 1000000000.0;
}

} // avs