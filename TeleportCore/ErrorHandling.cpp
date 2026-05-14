#include "TeleportCore/ErrorHandling.h"
#include "TeleportCore/Logging.h"
#include <array>
#include <cstdarg>  // for va_list, va_start, va_end
#include <cstdio>   // for vsnprintf
#ifdef _MSC_VER
	#include <Windows.h> // for DebugBreak
#elif defined __ANDROID__
	#include <signal.h>
#endif

void teleport::log_print_impl(const char* source, const std::string& message)
{
	std::cout << source << " " << message << std::endl;
}

namespace teleport
{
	static std::array<bool, 3> g_logCategoryEnabled = {
		false,	// Default
		true,	// Time
		true,	// Resource
	};

	bool IsLogCategoryEnabled(LogCategory category)
	{
		size_t idx = static_cast<size_t>(category);
		if (idx >= g_logCategoryEnabled.size())
			return false;
		return g_logCategoryEnabled[idx];
	}

	void SetLogCategoryEnabled(LogCategory category, bool enabled)
	{
		size_t idx = static_cast<size_t>(category);
		if (idx >= g_logCategoryEnabled.size())
			return;
		g_logCategoryEnabled[idx] = enabled;
	}
}

#if TELEPORT_INTERNAL_CHECKS
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

void teleport::DebugBreak()
{
#ifdef _MSC_VER
	::DebugBreak();
#elif defined __ANDROID__
	raise(SIGTRAP);
#endif
}
