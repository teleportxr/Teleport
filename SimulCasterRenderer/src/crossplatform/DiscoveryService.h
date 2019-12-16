#pragma once

#include <cstdint>

#include "enet/enet.h"

//Abstract discovery service for clients to connect to the server.
class DiscoveryService
{
public:
	virtual ~DiscoveryService(){};

	virtual bool Discover(uint16_t discoveryPort, ENetAddress& remote) = 0;
protected:
	uint32_t clientID = 0;
	int serviceDiscoverySocket = 0;
};