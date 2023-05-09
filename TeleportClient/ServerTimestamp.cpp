#include "ServerTimestamp.h"

using namespace teleport;

using namespace client;
	
uint64_t ServerTimestamp::lastReceivedTimestampUTCUnixNs;
double ServerTimestamp::currentTimestampUTCUnixNs;

void ServerTimestamp::setLastReceivedTimestampUTCUnixMs(uint64_t value)
{
	lastReceivedTimestampUTCUnixNs = value;
	currentTimestampUTCUnixNs = static_cast<double>(lastReceivedTimestampUTCUnixNs);
}

double ServerTimestamp::getCurrentTimestampUTCUnixNs()
{
	return currentTimestampUTCUnixNs;
}

double ServerTimestamp::getCurrentTimestampUTCUnixS()
{
	return double(currentTimestampUTCUnixNs)*0.000000001;
}

void ServerTimestamp::tick(double deltaTime_s)
{
	currentTimestampUTCUnixNs += deltaTime_s*1000000000.0;
}
