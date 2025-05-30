#pragma once

#include <map>
#include <set>
#include <string>
#include <libavstream/common.hpp>
#include <memory>
#include <map>
#include <nlohmann/json.hpp>
#include <safe/safe.h>
#include "TeleportServer/Export.h"
#include "TeleportCore/CommonNetworking.h"

namespace rtc
{
	class WebSocket;
	class WebSocketServer;
}

namespace teleport
{
	namespace server
	{
		struct TELEPORT_SERVER_API SignalingClient
		{
			~SignalingClient();
			avs::uid clientID = 0;
			std::string ip_addr_port;
			std::string path;
			std::shared_ptr<rtc::WebSocket> webSocket;
			std::vector<std::string> messagesReceived;
			std::queue<std::string> messagesToPassOn;
			std::queue<std::vector<uint8_t>> binaryMessagesReceived;
			core::SignalingState signalingState = core::SignalingState::START;
			std::mutex webSocketsMessagesMutex;
		};
		//! Signaling service for establishing connections with clients.
		class TELEPORT_SERVER_API SignalingService
		{
		public:
			~SignalingService()
			{
				shutdown();
			}
			bool initialize(uint64_t serverID, std::set<uint16_t> discoveryPorts, std::string desiredIP = "");
			void shutdown();
			void tick();
			void sendResponseToClient(std::shared_ptr<SignalingClient> &signalingClient, uint64_t clientID);
			void sendToClient(avs::uid clientID, std::string str);
			bool sendBinaryToClient(avs::uid clientID, std::vector<uint8_t> bin);
			void discoveryCompleteForClient(uint64_t clientID);
			const std::set<avs::uid> &getClientIds() const;
			std::shared_ptr<SignalingClient > getSignalingClient(avs::uid u);
		protected:
			using SignalingClientMap=std::map<avs::uid, std::shared_ptr<SignalingClient>>;
			void SetCallbacks(std::shared_ptr<SignalingClient> &signalingClient);
			void processDisconnection(avs::uid uid, safe::Safe<SignalingClientMap>::WriteAccess<> &writeSignalingClients, std::shared_ptr<SignalingClient> &signalingClient);
			void processInitialRequest(avs::uid clientID, safe::Safe<SignalingClientMap>::WriteAccess<> &writeSignalingClients, std::shared_ptr<SignalingClient> &discoveryClient, nlohmann::json &j);
			std::set<uint16_t> discoveryPorts;
			std::string desiredIP;
			safe::Safe<SignalingClientMap> signalingClients;
			std::map<avs::uid, avs::uid> clientRemapping;
			std::set<avs::uid> clientUids;
			std::map<uint16_t,std::shared_ptr<rtc::WebSocketServer>> webSocketServers;
			uint64_t serverID = 0;
		public:
			void OnWebSocket(std::shared_ptr<rtc::WebSocket>);
			void ReceiveWebSocketsMessage(avs::uid clientID, std::string msg);
			void ReceiveBinaryWebSocketsMessage(avs::uid clientID, std::vector<std::byte>& bin);
			bool GetNextMessage(avs::uid clientID, std::string &msg);
			bool GetNextBinaryMessage(avs::uid clientID, std::vector<uint8_t>& bin);
		};
	}
}