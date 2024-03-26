#pragma once

#include <cstdint>
#include <string>
// for std::future,await:
#include <future>
#include <queue>
#include <unordered_map>
#include "SignalingServer.h"

namespace rtc {
	class WebSocket;
}
namespace teleport
{
	namespace client
	{
		//! Abstract discovery service for clients to connect to the server.
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

		//	void SetClientID(uint64_t inClientID);
			bool GetNextMessage(uint64_t server_uid,std::string& msg);
			bool GetNextBinaryMessage(uint64_t server_uid,std::vector<uint8_t>& msg);
			void Send(uint64_t server_uid,std::string msg);
			void SendBinary(uint64_t server_uid, std::vector<uint8_t> bin);
			void Disconnect(uint64_t server_uid);
		protected:
			void Tick(uint64_t server_uid);
			void InitSocket(uint64_t server_uid);
		//! When e.g. ip address changes, reset the connection.
			void ResetConnection(uint64_t server_uid, std::string ip,uint16_t serverDiscoveryPort);
			std::mutex signalingServersMutex;
			void ReceiveWebSocketsMessage(uint64_t server_uid,std::string msg);
			void ReceiveBinaryWebSocketsMessage(uint64_t server_uid, std::vector<std::byte> bin);
			std::unordered_map<uint64_t, std::shared_ptr<SignalingServer>> signalingServers;
			int frame=1;
			std::vector<uint16_t> cyclePorts;
			friend struct SignalingServer;
		};
	}
}
