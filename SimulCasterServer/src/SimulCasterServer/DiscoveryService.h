#pragma once

#include <cstdint>

namespace SCServer
{
	class DiscoveryService
	{
	public:
		virtual bool initialise(uint16_t inDiscoveryPort = 0, uint16_t inServicePort = 0) = 0;
		virtual void shutdown() = 0;
		virtual void tick() = 0;
		virtual void sendResponseToClient(uint64_t clientID) = 0;
		virtual void discoveryCompleteForClient(uint64_t clientID) = 0;
	};
}