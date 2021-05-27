#pragma once

#include "TeleportClient/DiscoveryService.h"

#include "Config.h"

class AndroidDiscoveryService: public teleport::client::DiscoveryService
{
public:
    AndroidDiscoveryService(uint32_t manualClientID = TELEPORT_DEFAULT_CLIENT_ID);
    virtual ~AndroidDiscoveryService();

    virtual uint32_t Discover(std::string clientIP, uint16_t clientDiscoveryPort, std::string serverIP, uint16_t serverDiscoveryPort, ENetAddress& remote) override;
};
