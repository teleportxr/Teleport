#pragma once

#include "TeleportClient/DiscoveryService.h"

class AndroidDiscoveryService: public teleport::client::DiscoveryService
{
public:
    AndroidDiscoveryService();
    virtual ~AndroidDiscoveryService();

    virtual uint32_t Discover(std::string clientIP, uint16_t clientDiscoveryPort, std::string serverIP, uint16_t serverDiscoveryPort, ENetAddress& remote) override;
};