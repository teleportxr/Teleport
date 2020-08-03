#pragma once

#include "crossplatform/DiscoveryService.h"
#include <string>

class PCDiscoveryService: public DiscoveryService
{
public:
	PCDiscoveryService();
	virtual ~PCDiscoveryService();

	virtual uint32_t Discover(uint16_t clientDiscoveryPort, std::string serverIP, uint16_t serverDiscoveryPort, ENetAddress& remote) override;
protected:
	int CreateDiscoverySocket(uint16_t discoveryPort);
};