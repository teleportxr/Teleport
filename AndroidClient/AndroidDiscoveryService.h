#pragma once

#include "TeleportClient/DiscoveryService.h"
namespace teleport
{
	namespace android
	{
		class AndroidDiscoveryService: public teleport::client::DiscoveryService
		{
		public:
			AndroidDiscoveryService();
			virtual ~AndroidDiscoveryService();

			virtual uint64_t Discover1(std::string clientIP, uint16_t clientDiscoveryPort, std::string serverIP, uint16_t serverDiscoveryPort, ENetAddress& remote) ;
		};
	}
}