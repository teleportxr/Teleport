#pragma once

#include <map>
#include <set>
#include <enet/enet.h>
#include <string>
#include <libavstream/common.hpp>
#include <memory>
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
			OFFERING,		// Sent a WebRTC offer.
			STREAMING		// Completed signaling, now streaming. 

		};
		struct SignalingClient
		{
			std::string address;
			std::shared_ptr<rtc::WebSocket> webSocket;
			std::vector<std::string> messagesReceived;
			std::queue<std::string> messagesToPassOn;
			SignalingState signalingState = SignalingState::START;
		};
		//! Signaling service for establishing connections with clients.
		//! Discover service for establishing connections with clients.
		class SignalingService
		{
		public:
			~SignalingService()
			{
				shutdown();
			}
			bool initialize(uint16_t discoveryPort = 0, uint16_t servicePort = 0, std::string desiredIP = "");

			void shutdown();

			void tick();

			void sendResponseToClient(uint64_t clientID);
			void sendToClient(avs::uid clientID, std::string str);
			void discoveryCompleteForClient(uint64_t clientID);
			const std::set<avs::uid> &getClientIds() const;
			std::shared_ptr<SignalingClient > getSignalingClient(avs::uid u);
		protected:
			void processInitialRequest(avs::uid clientID, std::shared_ptr<SignalingClient> &discoveryClient,nlohmann::json& j);
			//List of clientIDs we want to attempt to connect to.
			std::map<uint64_t, ENetAddress> newClients;

			ENetSocket discoverySocket{};
			ENetAddress address{};

			uint16_t discoveryPort = 0;
			uint16_t servicePort = 0;
			std::string desiredIP;
			std::map<avs::uid, std::shared_ptr<SignalingClient>> signalingClients;
			std::set<avs::uid> clientUids;
			std::shared_ptr<rtc::WebSocketServer> webSocketServer;
			std::mutex webSocketsMessagesMutex;
		public:
			void OnWebSocket(std::shared_ptr<rtc::WebSocket>);
			void ReceiveWebSocketsMessage(avs::uid clientID, std::string msg); 
			bool GetNextMessage(avs::uid clientID, std::string &msg);
		};
		typedef SignalingService SignalingService;
	}
}