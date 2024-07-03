#include "TeleportCore/Time.h"
using namespace teleport;

using namespace core;
	
int64_t teleport::core::GetUnixTimeNs()
{
	const auto now = std::chrono::system_clock::now();
	std::chrono::nanoseconds ns = std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch());
			return ns.count();
}