#pragma once

#include "crossplatform/DiscoveryService.h"

class PCDiscoveryService: public DiscoveryService
{
public:
	PCDiscoveryService();
	virtual ~PCDiscoveryService();

	virtual bool Discover(uint16_t discoveryPort, ENetAddress& remote) override;
};