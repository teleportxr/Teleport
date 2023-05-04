#pragma once

#include <map>
#include <set>
#include <enet/enet.h>
#include <string>
#include <libavstream/common.hpp>
#include <memory>
#include <map>
#include <nlohmann/json.hpp>
#include "TeleportServer/UnityPlugin/Export.h"

namespace rtc
{
	class WebSocket;
	class WebSocketServer;
}

namespace teleport
{
	namespace server
	{
		enum class SignalingState
		{
			START,			// Received a WebSocket connection.
			REQUESTED,		// Got an initial connection request message
			ACCEPTED,		// Accepted the connection request. Create a ClientData for this client if not already extant.
			STREAMING,		// Completed initial signaling, now using the signaling socket for streaming setup.
			INVALID
		};
		struct SignalingClient
		{
			~SignalingClient();
			avs::uid clientID = 0;
			std::string ip_addr_port;
			std::shared_ptr<rtc::WebSocket> webSocket;
			std::vector<std::string> messagesReceived;
			std::queue<std::string> messagesToPassOn;
			std::queue<std::vector<uint8_t>> binaryMessagesReceived;
			SignalingState signalingState = SignalingState::START;
		};
		//! Signaling service for establishing connections with clients.
		class SignalingService
		{
		public:
			~SignalingService()
			{
				shutdown();
			}
			bool initialize(std::set<uint16_t> discoveryPorts, std::string desiredIP = "");
			void shutdown();
			void tick();
			void sendResponseToClient(uint64_t clientID);
			void sendToClient(avs::uid clientID, std::string str);
			bool sendBinaryToClient(avs::uid clientID, std::vector<uint8_t> bin);
			void discoveryCompleteForClient(uint64_t clientID);
			const std::set<avs::uid> &getClientIds() const;
			std::shared_ptr<SignalingClient > getSignalingClient(avs::uid u);
		protected:
			void SetCallbacks(std::shared_ptr<SignalingClient> &signalingClient);
			void processInitialRequest(avs::uid clientID, std::shared_ptr<SignalingClient> &discoveryClient,nlohmann::json& j);
			std::set<uint16_t> discoveryPorts;
			std::string desiredIP;
			std::map<avs::uid, std::shared_ptr<SignalingClient>> signalingClients;
			std::map<avs::uid, avs::uid> clientRemapping;
			std::set<avs::uid> clientUids;
			std::map<uint16_t,std::shared_ptr<rtc::WebSocketServer>> webSocketServers;
			std::mutex webSocketsMessagesMutex;
		public:
			void OnWebSocket(std::shared_ptr<rtc::WebSocket>);
			void ReceiveWebSocketsMessage(avs::uid clientID, std::string msg);
			void ReceiveBinaryWebSocketsMessage(avs::uid clientID, std::vector<std::byte>& bin);
			bool GetNextMessage(avs::uid clientID, std::string &msg);
			bool GetNextBinaryMessage(avs::uid clientID, std::vector<uint8_t>& bin);
		};
	}
}