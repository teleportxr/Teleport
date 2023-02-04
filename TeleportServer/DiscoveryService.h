#pragma once

#include <map>

#include <enet/enet.h>
#include <string>

#include "TeleportServer/UnityPlugin/Export.h"

namespace teleport
{
	//! Discover service for establishing connections with clients.
	class DiscoveryService
	{
	public:
		~DiscoveryService()
		{
			shutdown();
		}
		bool initialize(uint16_t discoveryPort = 0, uint16_t servicePort = 0, std::string desiredIP = "") ;

		void shutdown() ;

		void tick() ;

		void sendResponseToClient(uint64_t clientID) ;

		void discoveryCompleteForClient(uint64_t clientID) ;
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