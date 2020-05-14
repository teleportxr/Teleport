#pragma once

#include "crossplatform/DiscoveryService.h"

class PCDiscoveryService: public DiscoveryService
{
public:
	PCDiscoveryService();
	virtual ~PCDiscoveryService();

	virtual uint32_t Discover(uint16_t discoveryPort, ENetAddress& remote) override;
protected:
	int CreateDiscoverySocket(uint16_t discoveryPort) ;
};