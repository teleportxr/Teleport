#pragma once

#include "crossplatform/DiscoveryService.h"
#include <string>

class PCDiscoveryService: public DiscoveryService
{
public:
	PCDiscoveryService();
	virtual ~PCDiscoveryService();

	virtual uint32_t Discover(std::string clientIP, uint16_t clientDiscoveryPort, std::string serverIP, uint16_t serverDiscoveryPort, ENetAddress& remote) override;
protected:
	int CreateDiscoverySocket(std::string ip, uint16_t discoveryPort);
};