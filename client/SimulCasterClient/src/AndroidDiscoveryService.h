#pragma once

#include "crossplatform/DiscoveryService.h"

class AndroidDiscoveryService: public DiscoveryService
{
public:
    AndroidDiscoveryService();
    virtual ~AndroidDiscoveryService();

    virtual bool Discover(uint16_t discoveryPort, ENetAddress& remote) override;
};