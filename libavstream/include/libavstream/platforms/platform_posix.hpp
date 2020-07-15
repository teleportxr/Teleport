// libavstream
// (c) Copyright 2018 Simul.co

#pragma once

#include <time.h>
#include <libavstream/common.hpp>
#include <libavstream/platforms/platform_common.hpp>

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
        static double getTimeElapsed(const Timestamp& tBegin, const Timestamp& tEnd);
    };

    using Platform = PlatformPOSIX;
} // avs