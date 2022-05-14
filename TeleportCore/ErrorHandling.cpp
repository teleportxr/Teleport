#include "TeleportCore/ErrorHandling.h"

#if TELEPORT_INTERNAL_CHECKS
#ifdef _MSC_VER
#include <Windows.h>
#endif
void TeleportLogUnsafe(const char* fmt, ...)
{
    // we keep stack allocations to a minimum to keep logging side-effects to a minimum
    char msg[2048];
    // we use vsnprintf here rather than using __android_log_vprintf so we can control the size
    // and method of allocations as much as possible.
    va_list argPtr;
    va_start(argPtr, fmt);
    vsnprintf(msg, sizeof(msg), fmt, argPtr);
    va_end(argPtr);
#ifdef _MSC_VER
    OutputDebugStringA(msg);
#endif
}
#endif