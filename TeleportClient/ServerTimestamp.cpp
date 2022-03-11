#include "ServerTimestamp.h"

namespace teleport
{
	namespace client
	{
		uint64_t ServerTimestamp::lastReceivedTimestampUTCUnixMs;
		double ServerTimestamp::currentTimestampUTCUnixMs;

		void ServerTimestamp::setLastReceivedTimestampUTCUnixMs(uint64_t value)
		{
			lastReceivedTimestampUTCUnixMs = value;
			currentTimestampUTCUnixMs = static_cast<double>(lastReceivedTimestampUTCUnixMs);
		}

		double ServerTimestamp::getCurrentTimestampUTCUnixMs()
		{
			return currentTimestampUTCUnixMs;
		}

		void ServerTimestamp::tick(double deltaTime_s)
		{
			currentTimestampUTCUnixMs += deltaTime_s*1000.0;
		}
	}
}
