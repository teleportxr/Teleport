#pragma once

#include <map>

#include <enet/enet.h>

#include "SimulCasterServer/DiscoveryService.h"
#include "SimulCasterServer/UnityPlugin/Export.h"

namespace SCServer
{
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
		std::map<uint32_t, ENetAddress> newClients;

#pragma pack(push, 1) 
		struct ServiceDiscoveryResponse
		{
			uint32_t clientID;
			uint16_t remotePort;
		};
#pragma pack(pop)

		ENetSocket discoverySocket{};
		ENetAddress address{};

		uint16_t discoveryPort = 0;
		uint16_t servicePort = 0;
		std::string desiredIP;
	public:
	};
}