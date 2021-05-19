#include "ServerTimestamp.h"

namespace scr
{
	uint64_t ServerTimestamp::lastReceivedTimestamp;
	double ServerTimestamp::currentTimestamp;

	void ServerTimestamp::setLastReceivedTimestamp(uint64_t value)
	{
		lastReceivedTimestamp = value;
		currentTimestamp = static_cast<double>(lastReceivedTimestamp);
	}

	double ServerTimestamp::getCurrentTimestamp()
	{
		return currentTimestamp;
	}

	void ServerTimestamp::tick(double deltaTime)
	{
		currentTimestamp += deltaTime;
	}
}
