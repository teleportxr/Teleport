#pragma once

#include <map>

#include <enet/enet.h>

#include "TeleportServer/DiscoveryService.h"
#include "TeleportServer/UnityPlugin/Export.h"

namespace teleport
{
	//! Discover service for establishing connections with clients.
	class DefaultDiscoveryService: public DiscoveryService
	{
	public:
		~DefaultDiscoveryService()
		{
			shutdown();
		}
		bool initialize(uint16_t discoveryPort = 0, uint16_t servicePort = 0, std::string desiredIP = "") override;

		void shutdown() override;

		void tick() override;

		void sendResponseToClient(uint64_t clientID) override;

		void discoveryCompleteForClient(uint64_t clientID) override;
	protected:
		//List of clientIDs we want to attempt to connect to.
		std::map<uint64_t, ENetAddress> newClients;

		ENetSocket discoverySocket{};
		ENetAddress address{};

		uint16_t discoveryPort = 0;
		uint16_t servicePort = 0;
		std::string desiredIP;
	public:
	};
}