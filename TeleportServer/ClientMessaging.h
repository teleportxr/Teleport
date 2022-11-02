#pragma once

#include <atomic>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

#include "libavstream/common_input.h"

#include "CaptureDelegates.h"
#include "ServerSettings.h"
#include "TeleportCore/ErrorHandling.h"
#include "GeometryStreamingService.h"
#include "VideoEncodePipeline.h"
#include "TeleportCore/ErrorHandling.h"
#include "TeleportCore/Input.h"
#include "enet/enet.h"

typedef void(__stdcall* SetHeadPoseFn) (avs::uid uid, const avs::Pose*);
typedef void(__stdcall* SetControllerPoseFn) (avs::uid uid, int index, const avs::PoseDynamic*);
typedef void(__stdcall* ProcessNewInputFn) (avs::uid uid, const teleport::core::InputState*,const uint8_t **,const float **,const avs::InputEventBinary**, const avs::InputEventAnalogue**, const avs::InputEventMotion**);
typedef void(__stdcall* DisconnectFn) (avs::uid uid);
typedef void(__stdcall* ReportHandshakeFn) (avs::uid clientID,const teleport::core::Handshake *h);

//typedef struct _ENetPeer ENetPeer;
//typedef struct _ENetPacket ENetPacket;
//typedef struct _ENetEvent ENetEvent;

namespace teleport
{
	class DiscoveryService;
	class ClientManager;
	//! Per-client messaging handler.
	class ClientMessaging
	{
	public:
		ClientMessaging(const struct ServerSettings* settings,
						std::shared_ptr<DiscoveryService> discoveryService,
						SetHeadPoseFn setHeadPose,
						SetControllerPoseFn setControllerPose,
						ProcessNewInputFn processNewInput,
						DisconnectFn onDisconnect,
						uint32_t disconnectTimeout,
						ReportHandshakeFn reportHandshakeFn,
						ClientManager* clientManager);
		
		virtual ~ClientMessaging();

		bool isInitialised() const;
		void initialise(CasterContext* context, CaptureDelegates captureDelegates);
		void unInitialise();

		bool startSession(avs::uid clientID, std::string clientIP);
		void stopSession();
		bool restartSession(avs::uid clientID, std::string clientIP);
		bool isStartingSession() { return startingSession;  }
		void tick(float deltaTime);
		void handleEvents(float deltaTime);
		void Disconnect();

		void nodeEnteredBounds(avs::uid nodeID);
		void nodeLeftBounds(avs::uid nodeID);
		void updateNodeMovement(const std::vector<teleport::core::MovementUpdate>& updateList);
		void updateNodeEnabledState(const std::vector<teleport::core::NodeUpdateEnabledState>& updateList);
		void setNodeHighlighted(avs::uid nodeID, bool isHighlighted);
		void reparentNode(avs::uid nodeID, avs::uid newParentID,avs::Pose relPose);
		void setNodePosePath(avs::uid nodeID, const std::string &regexPosePath);
		void updateNodeAnimation(teleport::core::ApplyAnimation update);
		void updateNodeAnimationControl(teleport::core::NodeUpdateAnimationControl update);
		void updateNodeRenderState(avs::uid nodeID,avs::NodeRenderState update);
		void setNodeAnimationSpeed(avs::uid nodeID, avs::uid animationID, float speed);

		bool hasPeer() const;
		bool hasReceivedHandshake() const;

		bool setOrigin(uint64_t valid_counter,avs::uid originNode,const avs::vec3 &pos,const avs::vec4 &orientation);
		template<typename C> bool sendCommand(const C& command) const
		{
			if(!peer)
			{
				TELEPORT_CERR << "Failed to send command with type: " << static_cast<int>(command.commandPayloadType) << "! ClientMessaging has no peer!\n";
				return false;
			}

			size_t commandSize = sizeof(C);
			ENetPacket* packet = enet_packet_create(&command, commandSize, ENET_PACKET_FLAG_RELIABLE);
	
			if(!packet)
			{
				TELEPORT_CERR << "Failed to send command with type: " << static_cast<int>(command.commandPayloadType) << "! Failed to create packet!\n";
				return false;
			}

			return enet_peer_send(peer, static_cast<enet_uint8>(teleport::core::RemotePlaySessionChannel::RPCH_Control), packet) == 0;
		}

		uint16_t getServerPort() const;

		uint16_t getStreamingPort() const;

		template<typename C,typename T> bool sendCommand(const C& command, const std::vector<T>& appendedList) const
		{
			if(!peer)
			{
				TELEPORT_CERR << "Failed to send command with type: " << static_cast<uint8_t>(command.commandPayloadType) << "! ClientMessaging has no peer!\n";
				return false;
			}

			size_t commandSize = sizeof(C);
			size_t listSize = sizeof(T) * appendedList.size();

			ENetPacket* packet = enet_packet_create(&command, commandSize, ENET_PACKET_FLAG_RELIABLE);
			
			if(!packet)
			{
				TELEPORT_CERR << "Failed to send command with type: " << static_cast<uint8_t>(command.commandPayloadType) << "! Failed to create packet!\n";
				return false;
			}

			//Copy list into packet.
			enet_packet_resize(packet, commandSize + listSize);
			memcpy(packet->data + commandSize, appendedList.data(), listSize);
			
			return enet_peer_send(peer, static_cast<enet_uint8>(teleport::core::RemotePlaySessionChannel::RPCH_Control), packet) == 0;
		}
		template <> bool sendCommand<teleport::core::SetupInputsCommand,teleport::core::InputDefinition>(const teleport::core::SetupInputsCommand& command, const std::vector<teleport::core::InputDefinition>& appendedInputDefinitions) const
		{
			if (!peer)
			{
				TELEPORT_CERR << "Failed to send command with type: " << static_cast<uint8_t>(command.commandPayloadType) << "! ClientMessaging has no peer!\n";
				return false;
			}
			if (command.commandPayloadType != teleport::core::CommandPayloadType::SetupInputs)
			{
				TELEPORT_CERR << "Invalid command!\n";
				return false;
			}
			size_t commandSize = sizeof(teleport::core::SetupInputsCommand);
			size_t listSize = appendedInputDefinitions.size()*(sizeof(avs::InputId)+sizeof(avs::InputType));
			for (const auto& d : appendedInputDefinitions)
			{
				// a uint16 to store the path length.
				listSize += sizeof(uint16_t);
				// and however many chars in the path.
				listSize += sizeof(char)*d.regexPath.length();
				if (d.regexPath.length() >= (1 << 16))
				{
					TELEPORT_CERR << "Input path too long!\n";
					return false;
				}
			}
			ENetPacket* packet = enet_packet_create(&command, commandSize, ENET_PACKET_FLAG_RELIABLE);
			if (!packet)
			{
				TELEPORT_CERR << "Failed to send command with type: " << static_cast<uint8_t>(command.commandPayloadType) << "! Failed to create packet!\n";
				return false;
			}
			//Copy list into packet.
			enet_packet_resize(packet, commandSize + listSize);
			unsigned char* data_ptr = packet->data + commandSize;
			for (const auto& d : appendedInputDefinitions)
			{
				teleport::core::InputDefinitionNetPacket defPacket;
				defPacket.inputId = d.inputId;
				defPacket.inputType = d.inputType;
				defPacket.pathLength= (uint16_t)d.regexPath.length();
				memcpy(data_ptr, &defPacket, sizeof(defPacket));
				data_ptr += sizeof(defPacket);
				memcpy(data_ptr, d.regexPath.c_str(), d.regexPath.length());
				data_ptr += d.regexPath.length();
			}
			if (packet->data + commandSize + listSize != data_ptr)
			{
				TELEPORT_CERR << "Failed to send command due to packet size discrepancy\n";
				return false;
			}

			return enet_peer_send(peer, static_cast<enet_uint8>(teleport::core::RemotePlaySessionChannel::RPCH_Control), packet) == 0;
		}
		std::string getClientIP() const 
		{
			return clientIP;
		};

		avs::uid getClientID() const
		{
			return clientID;
		}

		// Same as c;ient ip.
		std::string getPeerIP() const;

		uint16_t getClientPort() const;

		CasterContext* getContext() const
		{
			return casterContext;
		}

		bool timedOutStartingSession() const;

		void ConfirmSessionStarted();

		GeometryStreamingService &GetGeometryStreamingService()
		{
			return geometryStreamingService;
		}
	private:
		friend class ClientManager;
		void dispatchEvent(const ENetEvent& event);
		void receiveHandshake(const ENetPacket* packet);
		void receiveInput(const ENetPacket* packet);
		void receiveDisplayInfo(const ENetPacket* packet);
		void receiveHeadPose(const ENetPacket* packet);
		void receiveResourceRequest(const ENetPacket* packet);
		void receiveKeyframeRequest(const ENetPacket* packet);
		void receiveClientMessage(const ENetPacket* packet);
		
		avs::ThreadSafeQueue<ENetEvent> eventQueue;
		teleport::core::Handshake handshake;
		static bool asyncNetworkDataProcessingFailed;
		avs::uid clientID;
		std::string clientIP;
		bool initialized=false;
		bool startingSession;
		float timeStartingSession;
		float timeSinceLastClientComm;
		const ServerSettings* settings;
		std::shared_ptr<DiscoveryService> discoveryService;
		PluginGeometryStreamingService geometryStreamingService;
		ClientManager* clientManager;
		SetHeadPoseFn setHeadPose; //Delegate called when a head pose is received.
		SetControllerPoseFn setControllerPose; //Delegate called when a head pose is received.
		ProcessNewInputFn processNewInput; //Delegate called when new input is received.
		DisconnectFn onDisconnect; //Delegate called when the peer disconnects.
		ReportHandshakeFn reportHandshake;

		uint32_t disconnectTimeout;
		uint16_t streamingPort;

		CasterContext* casterContext;
		CaptureDelegates captureComponentDelegates;

		ENetPeer* peer=nullptr;

		std::atomic_bool receivedHandshake = false;				//Whether we've received the handshake from the client.

		std::vector<avs::uid> nodesEnteredBounds;	//Stores nodes client needs to know have entered streaming bounds.
		std::vector<avs::uid> nodesLeftBounds;		//Stores nodes client needs to know have left streaming bounds.

		core::Input latestInputStateAndEvents; //Latest input state received from the client.

		// Seconds
		static constexpr float startSessionTimeout = 3;
	};
}