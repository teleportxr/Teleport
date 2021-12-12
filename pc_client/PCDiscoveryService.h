#pragma once

#include "TeleportClient/DiscoveryService.h"

#include "libavstream/common_networking.h"

#include <string>
// for await:
#include <future>

class PCDiscoveryService: public teleport::client::DiscoveryService
{
public:
	PCDiscoveryService(uint32_t manualClientID = 0);
	virtual ~PCDiscoveryService();

	virtual uint32_t Discover(std::string clientIP, uint16_t clientDiscoveryPort, std::string serverIP, uint16_t serverDiscoveryPort, ENetAddress& remote) override;
protected:
	ENetSocket CreateDiscoverySocket(std::string ip, uint16_t discoveryPort);
	bool awaiting = false;
	std::future<int> fobj;
	ENetAddress serverAddress;
	std::string serverIP;
};