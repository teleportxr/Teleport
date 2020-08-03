#pragma once

#include "crossplatform/DiscoveryService.h"

class AndroidDiscoveryService: public DiscoveryService
{
public:
    AndroidDiscoveryService();
    virtual ~AndroidDiscoveryService();

    virtual uint32_t Discover(uint16_t clientDiscoveryPort, std::string serverIP, uint16_t serverDiscoveryPort, ENetAddress& remote) override;
};