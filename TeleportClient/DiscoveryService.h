#pragma once

#include <cstdint>
#include <string>
// for std::future,await:
#include <future>
#include <queue>
#include <unordered_map>
#include "enet/enet.h"

namespace rtc {
	class WebSocket;
}
namespace teleport
{
	namespace client
	{
		struct SignalingServer
		{
			std::string url;
			uint16_t remotePort=0;
			uint64_t uid=0;
			uint8_t cyclePortIndex= 0;
			std::shared_ptr<rtc::WebSocket> webSocket;
			uint16_t GetPort() const;
		};
		//Abstract discovery service for clients to connect to the server.
		class DiscoveryService
		{
		public:
			DiscoveryService();
			virtual ~DiscoveryService();
			static DiscoveryService &GetInstance();
			static void ShutdownInstance();
			/// Returns Client ID.
			uint64_t Discover(uint64_t server_uid, std::string serverIP, uint16_t serverDiscoveryPort);
			void Tick();

			void SetClientID(uint64_t inClientID);
			bool GetNextMessage(uint64_t server_uid,std::string& msg);
			bool GetNextBinaryMessage(uint64_t server_uid,std::vector<uint8_t>& msg);
			void Send(uint64_t server_uid,std::string msg);
			void SendBinary(uint64_t server_uid, std::vector<uint8_t> bin);

		protected:
			void Tick(uint64_t server_uid);
			void InitSocket(uint64_t server_uid);
		//! When e.g. ip address changes, reset the connection.
			void ResetConnection(uint64_t server_uid, std::string ip,uint16_t serverDiscoveryPort);
			uint64_t clientID = uint64_t(0x0);
			bool awaiting = false;
			std::mutex mutex;
			void ReceiveWebSocketsMessage(uint64_t server_uid,std::string msg);
			void ReceiveBinaryWebSocketsMessage(uint64_t server_uid,std::vector<std::byte> bin);
			std::unordered_map<uint64_t, std::shared_ptr<SignalingServer>> signalingServers;
			std::queue<std::string> messagesReceived;
			std::queue<std::string> messagesToPassOn;
			std::queue<std::string> messagesToSend;
			std::queue<std::vector<std::byte>> binaryMessagesReceived;
			std::queue<std::vector<std::byte>> binaryMessagesToSend;
			int frame=1;
			std::vector<uint16_t> cyclePorts;
			friend struct SignalingServer;
		};
	}
}
