#pragma once

#include <cstdint>
#include <chrono>
// Get the current Unix time in microseconds.
// this is the time since the Unix epoch (January 1, 1970 00:00:00 UTC).
namespace teleport
{
	namespace core
	{
		inline int64_t GetUnixTimeUs()
		{
			const auto now = std::chrono::system_clock::now();
			std::chrono::microseconds us = std::chrono::duration_cast<std::chrono::microseconds>(now.time_since_epoch());
			return us.count();
		}
	}
}