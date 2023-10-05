// libavstream
// (c) Copyright 2018 Simul.co

#pragma once

#include <time.h>
#include <libavstream/common.hpp>
#include <libavstream/platforms/platform_common.hpp>
#include <cstring>

inline void memcpy_s(void *targ, [[maybe_unused]] size_t size1, void *src, size_t size2)
{
	assert(size1<=size2);
	memcpy(targ,src,size2);
}

namespace avs
{
    typedef void* LibraryHandle;
    typedef void* ProcAddress;
    typedef struct timespec Timestamp;

    class PlatformPOSIX
    {
    public:
        static LibraryHandle openLibrary(const char* filename);
        static bool closeLibrary(LibraryHandle hLibrary);
        static ProcAddress getProcAddress(LibraryHandle hLibrary, const char* function);

        static Timestamp getTimestamp();
        static double getTimeElapsedInMilliseconds(const Timestamp& tBegin, const Timestamp& tEnd);
        static double getTimeElapsedInSeconds(const Timestamp& tBegin, const Timestamp& tEnd);
    };

    using Platform = PlatformPOSIX;
} // avs