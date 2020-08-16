#pragma once

#include <cstdint>
#include <string>
#include "enet/enet.h"

//Abstract discovery service for clients to connect to the server.
class DiscoveryService
{
public:
	virtual ~DiscoveryService(){};
	/// Returns Client ID.
	virtual uint32_t Discover(std::string clientIP, uint16_t clientDiscoveryPort, std::string serverIP, uint16_t serverDiscoveryPort, ENetAddress& remote) = 0;
protected:
	uint32_t clientID = 0;
	int serviceDiscoverySocket = 0;
};