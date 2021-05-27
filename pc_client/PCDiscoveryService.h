#pragma once

#include "TeleportClient/DiscoveryService.h"

#include <string>

#include "Config.h"

class PCDiscoveryService: public teleport::client::DiscoveryService
{
public:
	PCDiscoveryService(uint32_t manualClientID = TELEPORT_DEFAULT_CLIENT_ID);
	virtual ~PCDiscoveryService();

	virtual uint32_t Discover(std::string clientIP, uint16_t clientDiscoveryPort, std::string serverIP, uint16_t serverDiscoveryPort, ENetAddress& remote) override;
protected:
	int CreateDiscoverySocket(std::string ip, uint16_t discoveryPort);
};