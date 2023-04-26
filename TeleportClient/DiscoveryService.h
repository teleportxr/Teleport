#pragma once

#include <cstdint>
#include <string>
// for std::future,await:
#include <future>
#include <queue>
#include "enet/enet.h"

namespace rtc {
	class WebSocket;
}
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
			static DiscoveryService &GetInstance();
			static void ShutdownInstance();
			/// Returns Client ID.
			virtual uint64_t Discover(uint64_t server_uid,std::string clientIP, uint16_t clientDiscoveryPort, std::string serverIP, uint16_t serverDiscoveryPort, ENetAddress& remote);

			void SetClientID(uint64_t inClientID);
			bool GetNextMessage(uint64_t server_uid,std::string& msg);
			void Send(uint64_t server_uid,std::string msg);
		protected:
			uint64_t clientID = uint64_t(0x0);
			bool awaiting = false;
			std::future<int> fobj;
			std::mutex mutex;
			ENetAddress serverAddress;
			uint16_t remotePort=0;
			std::string serverIP;
			ENetSocket CreateDiscoverySocket(std::string ip, uint16_t discoveryPort);
			std::unordered_map<uint64_t,std::shared_ptr<rtc::WebSocket>> websockets;
			void ReceiveWebSocketsMessage(uint64_t server_uid,std::string msg);
			std::queue<std::string> messagesReceived;
			std::queue<std::string> messagesToPassOn;
			std::queue<std::string> messagesToSend;
		};
	}
}
