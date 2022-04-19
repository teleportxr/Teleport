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
#include "enet/enet.h"

typedef void(__stdcall* SetHeadPoseFn) (avs::uid uid, const avs::Pose*);
typedef void(__stdcall* SetOriginFromClientFn) (avs::uid uid, uint64_t, const avs::Pose*);
typedef void(__stdcall* SetControllerPoseFn) (avs::uid uid, int index, const avs::Pose*);
typedef void(__stdcall* ProcessNewInputFn) (avs::uid uid, const avs::InputState*, const avs::InputEventBinary**, const avs::InputEventAnalogue**, const avs::InputEventMotion**);
typedef void(__stdcall* DisconnectFn) (avs::uid uid);
typedef void(__stdcall* ReportHandshakeFn) (avs::uid clientID,const avs::Handshake *h);

//typedef struct _ENetPeer ENetPeer;
//typedef struct _ENetPacket ENetPacket;
//typedef struct _ENetEvent ENetEvent;

namespace teleport
{
	class DiscoveryService;
	class ClientManager;
	class ClientMessaging
	{
	public:
		ClientMessaging(const struct ServerSettings* settings,
						std::shared_ptr<DiscoveryService> discoveryService,
						std::shared_ptr<GeometryStreamingService> geometryStreamingService,
						SetHeadPoseFn setHeadPose,
						SetOriginFromClientFn setOriginFromClient,
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
		void updateNodeMovement(const std::vector<avs::MovementUpdate>& updateList);
		void updateNodeEnabledState(const std::vector<avs::NodeUpdateEnabledState>& updateList);
		void setNodeHighlighted(avs::uid nodeID, bool isHighlighted);
		void reparentNode(avs::uid nodeID, avs::uid newParentID,avs::Pose relPose);
		void setNodeSubtype(avs::uid nodeID, avs::NodeSubtype subType,const std::string &regexPosePath);
		void updateNodeAnimation(avs::ApplyAnimation update);
		void updateNodeAnimationControl(avs::NodeUpdateAnimationControl update);
		void updateNodeRenderState(avs::uid nodeID,avs::NodeRenderState update);
		void setNodeAnimationSpeed(avs::uid nodeID, avs::uid animationID, float speed);

		bool hasPeer() const;
		bool hasReceivedHandshake() const;

		bool setPosition(uint64_t valid_counter,const avs::vec3 &pos,bool set_rel,const avs::vec3 &rel_to_head,const avs::vec4 &orientation);

		bool sendCommand(const avs::Command& avsCommand) const;

		uint16_t getServerPort() const;

		uint16_t getStreamingPort() const;

		template<typename T> bool sendCommand(const avs::Command& command, const std::vector<T>& appendedList) const
		{
			if(!peer)
			{
				TELEPORT_CERR << "Failed to send command with type: " << static_cast<uint8_t>(command.commandPayloadType) << "! ClientMessaging has no peer!\n";
				return false;
			}

			size_t commandSize = command.getCommandSize();
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
			
			return enet_peer_send(peer, static_cast<enet_uint8>(avs::RemotePlaySessionChannel::RPCH_Control), packet) == 0;
		}
		template <> bool sendCommand<avs::InputDefinition>(const avs::Command& command, const std::vector<avs::InputDefinition>& appendedInputDefinitions) const
		{
			if (!peer)
			{
				TELEPORT_CERR << "Failed to send command with type: " << static_cast<uint8_t>(command.commandPayloadType) << "! ClientMessaging has no peer!\n";
				return false;
			}
			if (command.commandPayloadType != avs::CommandPayloadType::SetupInputs)
			{
				TELEPORT_CERR << "Invalid command!\n";
				return false;
			}
			size_t commandSize = command.getCommandSize();
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
				avs::InputDefinitionNetPacket defPacket;
				defPacket.inputId = d.inputId;
				defPacket.inputType = d.inputType;
				defPacket.pathLength= (uint16_t)d.regexPath.length();
				memcpy(data_ptr, &defPacket, sizeof(defPacket));
				data_ptr += sizeof(defPacket);
				memcpy(data_ptr, d.regexPath.c_str(),  d.regexPath.length());
				data_ptr += d.regexPath.length();
			}
			if (packet->data + commandSize + listSize != data_ptr)
			{
				TELEPORT_CERR << "Failed to send command due to packet size discrepancy\n";
				return false;
			}

			return enet_peer_send(peer, static_cast<enet_uint8>(avs::RemotePlaySessionChannel::RPCH_Control), packet) == 0;
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
		avs::Handshake handshake;
		static bool asyncNetworkDataProcessingFailed;
		avs::uid clientID;
		std::string clientIP;
		bool initialized=false;
		bool startingSession;
		float timeStartingSession;
		float timeSinceLastClientComm;
		const ServerSettings* settings;
		std::shared_ptr<DiscoveryService> discoveryService;
		std::shared_ptr<GeometryStreamingService> geometryStreamingService;
		ClientManager* clientManager;
		SetHeadPoseFn setHeadPose; //Delegate called when a head pose is received.
		SetOriginFromClientFn setOriginFromClient; //Delegate called when an origin is received.
		SetControllerPoseFn setControllerPose; //Delegate called when a head pose is received.
		ProcessNewInputFn processNewInput; //Delegate called when new input is received.
		DisconnectFn onDisconnect; //Delegate called when the peer disconnects.
		ReportHandshakeFn reportHandshake;

		uint32_t disconnectTimeout;
		uint16_t streamingPort;

		CasterContext* casterContext;
		CaptureDelegates captureComponentDelegates;

		ENetPeer* peer;

		std::atomic_bool receivedHandshake = false;				//Whether we've received the handshake from the client.

		std::vector<avs::uid> nodesEnteredBounds;	//Stores nodes client needs to know have entered streaming bounds.
		std::vector<avs::uid> nodesLeftBounds;		//Stores nodes client needs to know have left streaming bounds.

		struct InputStateAndEvents
		{
			avs::InputState inputState;
			//New input events we have received from the client this tick.
			std::vector<avs::InputEventBinary> binaryEvents;
			std::vector<avs::InputEventAnalogue> analogueEvents;
			std::vector<avs::InputEventMotion> motionEvents;
			void clear()
			{
				inputState.numBinaryEvents = 0;
				inputState.numAnalogueEvents = 0;
				inputState.numMotionEvents = 0;
				binaryEvents.clear();
				analogueEvents.clear();
				motionEvents.clear();
			}
		};
		InputStateAndEvents latestInputStateAndEvents; //Latest input state received from the client.

		// Seconds
		static constexpr float startSessionTimeout = 3;
	};
}