#include "ClientTime.h"
#include <chrono>
using namespace teleport;

using namespace client;
	
ClientTime::ClientTime()
{
	// on initialization, get the current Unix time in microseconds.
	// this is the time since the Unix epoch (January 1, 1970 00:00:00 UTC).
	const auto now = std::chrono::system_clock::now();
	auto us = std::chrono::duration_cast<std::chrono::microseconds>(now.time_since_epoch());
	start_microseconds_since_epoch =us.count();
}

ClientTime& ClientTime::GetInstance()
{
	static ClientTime clientTime;
	return clientTime;
}

double ClientTime::GetTimeS() const
{
	long long microseconds_since_start = GetTimeUs();
	double seconds_since_start = static_cast<double>(microseconds_since_start) / 1000000.0;
	return seconds_since_start;
}

long long ClientTime::GetTimeUs() const
{
	const auto now = std::chrono::system_clock::now();
	auto us = std::chrono::duration_cast<std::chrono::microseconds>(now.time_since_epoch());
	long long microseconds_since_epoch = us.count();
	long long microseconds_since_start = microseconds_since_epoch - start_microseconds_since_epoch;
	return microseconds_since_start;
}

long long ClientTime::GetTimeNs() const
{
	return GetTimeUs()*1000;
}

double ClientTime::ClientToServerTimeS(long long server_start_unix_time_us, double clientTimeS) const
{
	// What is the server time for this client time?
	long long client_time_us = static_cast<long long>(clientTimeS * 1000000.0);
	long long unix_time_us= start_microseconds_since_epoch+ client_time_us;
	long long server_time_us = unix_time_us -  server_start_unix_time_us;
	double server_time_s = double(server_time_us) / 1000000.0;
	return server_time_s;
}