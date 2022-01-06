#pragma once

#include <cstdint>
#include <string>
#include "enet/enet.h"

namespace teleport
{
	namespace client
	{
		//Abstract discovery service for clients to connect to the server.
		class DiscoveryService
		{
		public:
			DiscoveryService(uint32_t manualClientID = 0)
			{
				// TODO: client ID should come from the server.
				if (manualClientID == 0)
				{
					clientID = 0;
				}
				else
				{
					clientID = manualClientID;
				}
			}
			virtual ~DiscoveryService(){};
			/// Returns Client ID.
			virtual uint32_t Discover(std::string clientIP, uint16_t clientDiscoveryPort, std::string serverIP, uint16_t serverDiscoveryPort, ENetAddress& remote) = 0;

			void SetClientID(uint32_t inClientID)
			{
				if (inClientID != 0)
				{
					clientID = inClientID;
				}
			}
		protected:
			uint32_t clientID = 0;
			ENetSocket serviceDiscoverySocket = 0;
		};
	}
}
