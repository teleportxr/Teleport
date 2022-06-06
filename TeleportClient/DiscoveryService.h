#pragma once

#include <cstdint>
#include <string>
// for std::future,await:
#include <future>
#include "enet/enet.h"

namespace teleport
{
	namespace client
	{
		//Abstract discovery service for clients to connect to the server.
		class DiscoveryService
		{
		public:
			DiscoveryService();
			virtual ~DiscoveryService();
			/// Returns Client ID.
			virtual uint64_t Discover(std::string clientIP, uint16_t clientDiscoveryPort, std::string serverIP, uint16_t serverDiscoveryPort, ENetAddress& remote);

			void SetClientID(uint64_t inClientID);
		protected:
			uint64_t clientID = uint64_t(0x0);
			ENetSocket serviceDiscoverySocket = 0;
			bool awaiting = false;
			std::future<int> fobj;
			ENetAddress serverAddress;
			std::string serverIP;
			ENetSocket CreateDiscoverySocket(std::string ip, uint16_t discoveryPort);
		};
	}
}
