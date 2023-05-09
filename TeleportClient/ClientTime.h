#pragma once

#include <cstdint>

namespace teleport
{
	namespace client
	{
		//! A static-only class to hold and manage server timestamps.
		class ClientTime
		{
		public:
			ClientTime();
			static ClientTime& GetInstance();
			//! Get the time since start, in seconds, to nanosecond accuracy.
			double GetTimeS() const;
			long long GetTimeNs() const;
			double ClientToServerTimeS(long long server_start_unix_time_ns,double clientTimeS) const;
		protected:
			long long start_nanoseconds_since_epoch = 0;
		};
	}
}