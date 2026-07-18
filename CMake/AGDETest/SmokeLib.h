#pragma once

#include <string>

namespace smoke
{
	// Returns a description of the build environment; exercises C++20 std::format,
	// which requires NDK r27+ libc++ on Android.
	std::string Describe(int value);
}
