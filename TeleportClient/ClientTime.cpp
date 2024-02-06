#include "ClientTime.h"
#include <chrono>
using namespace teleport;

using namespace client;
	
ClientTime::ClientTime()
{
	// on initialization, get the current Unix time in nanoseconds.
	// this is the time since the Unix epoch (January 1, 1970 00:00:00 UTC).
	const auto now = std::chrono::system_clock::now();
	auto ns=std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch());
	start_nanoseconds_since_epoch =ns.count();
}

ClientTime& ClientTime::GetInstance()
{
	static ClientTime clientTime;
	return clientTime;
}

double ClientTime::GetTimeS() const
{
	long long nanoseconds_since_start = GetTimeNs();
	double seconds_since_start = static_cast<double>(nanoseconds_since_start) / 1000000000.0;
	return seconds_since_start;
}

long long ClientTime::GetTimeNs() const
{
	const auto now = std::chrono::system_clock::now();
	auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch());
	long long nanoseconds_since_epoch = ns.count();
	long long nanoseconds_since_start = nanoseconds_since_epoch - start_nanoseconds_since_epoch;
	return nanoseconds_since_start;
}

double ClientTime::ClientToServerTimeS(long long server_start_unix_time_us, double clientTimeS) const
{
	// What is the server time for this client time?
	long long client_time_ns = static_cast<long long>(clientTimeS * 1000000000.0);
	long long unix_time_ns= start_nanoseconds_since_epoch+ client_time_ns;
	long long server_time_ns = unix_time_ns - (long long)(1000 * server_start_unix_time_us);
	double server_time_s = double(server_time_ns) / 1000000000.0;
	return server_time_s;
}