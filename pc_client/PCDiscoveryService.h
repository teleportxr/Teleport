#pragma once

#include "TeleportClient/DiscoveryService.h"

class PCDiscoveryService: public teleport::client::DiscoveryService
{
public:
	PCDiscoveryService();
	~PCDiscoveryService();
};