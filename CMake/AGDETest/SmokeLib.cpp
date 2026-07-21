#include "SmokeLib.h"

#include <format>

namespace smoke
{
	std::string Describe(int value)
	{
#if defined(__ANDROID__)
		return std::format("AGDE smoke test on Android API {}: value={}", __ANDROID_API__, value);
#else
		return std::format("AGDE smoke test (host build): value={}", value);
#endif
	}
}
