#pragma once

#include <atomic>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

#include "libavstream/common_input.h"

#include "CaptureDelegates.h"
#include "CasterSettings.h"
#include "ErrorHandling.h"
#include "GeometryStreamingService.h"
#include "VideoEncodePipeline.h"

typedef void(__stdcall* SetHeadPoseFn) (avs::uid uid, const avs::Pose*);
typedef void(__stdcall* SetOriginFromClientFn) (avs::uid uid, uint64_t, const avs::Pose*);
typedef void(__stdcall* SetControllerPoseFn) (avs::uid uid, int index, const avs::Pose*);
typedef void(__stdcall* ProcessNewInputFn) (avs::uid uid, const avs::InputState*, const avs::InputEventBinary**, const avs::InputEventAnalogue**, const avs::InputEventMotion**);
typedef void(__stdcall* DisconnectFn) (avs::uid uid);
typedef void(__stdcall* ReportHandshakeFn) (avs::uid clientID,const avs::Handshake *h);

typedef struct _ENetHost ENetHost;
typedef struct _ENetPeer ENetPeer;
typedef struct _ENetPacket ENetPacket;
typedef struct _ENetEvent ENetEvent;

namespace SCServer
{
	class DiscoveryService;

	class ClientMessaging
	{
	public:
		ClientMessaging(const struct CasterSettings* settings,
						std::shared_ptr<DiscoveryService> discoveryService,
						std::shared_ptr<GeometryStreamingService> geometryStreamingService,
						SetHeadPoseFn setHeadPose,
						SetOriginFromClientFn setOriginFromClient,
						SetControllerPoseFn setControllerPose,
						ProcessNewInputFn processNewInput,
						DisconnectFn onDisconnect,
						const uint32_t& disconnectTimeout,
						ReportHandshakeFn reportHandshakeFn);
		
		virtual ~ClientMessaging();

		bool isInitialised() const;
		void initialise(CasterContext* context, CaptureDelegates captureDelegates);
		void unInitialise();

		bool startSession(avs::uid clientID, int32_t listenPort);
		void stopSession();
		bool restartSession(avs::uid clientID, int32_t listenPort);
		bool isStartingSession() { return startingSession;  }
		void tick(float deltaTime);
		void handleEvents(float deltaTime);
		void Disconnect();

		void nodeEnteredBounds(avs::uid nodeID);
		void nodeLeftBounds(avs::uid nodeID);
		void updateNodeMovement(const std::vector<avs::MovementUpdate>& updateList);
		void updateNodeEnabledState(const std::vector<avs::NodeUpdateEnabledState>& updateList);
		void updateNodeAnimation(avs::NodeUpdateAnimation update);
		void updateNodeAnimationControl(avs::NodeUpdateAnimationControl update);
		void setNodeHighlighted(avs::uid nodeID, bool isHighlighted);

		bool hasHost() const;
		bool hasPeer() const;
		bool hasReceivedHandshake() const;

		bool setPosition(uint64_t valid_counter,const avs::vec3 &pos,bool set_rel,const avs::vec3 &rel_to_head,const avs::vec4 &orientation);

		bool sendCommand(const avs::Command& avsCommand) const;
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

		std::string getClientIP() const;
		uint16_t getClientPort() const;
		uint16_t getServerPort() const;

		static void startAsyncNetworkDataProcessing();
		static void stopAsyncNetworkDataProcessing(bool killThread = true);

		static avs::Timestamp getLastTickTimestamp();

	private:
		static bool asyncNetworkDataProcessingFailed;

		avs::uid clientID;
		bool initialized=false;
		bool startingSession;
		float timeSinceLastClientComm;
		const CasterSettings* settings;
		std::shared_ptr<DiscoveryService> discoveryService;
		std::shared_ptr<GeometryStreamingService> geometryStreamingService;

		SetHeadPoseFn setHeadPose; //Delegate called when a head pose is received.
		SetOriginFromClientFn setOriginFromClient; //Delegate called when an origin is received.
		SetControllerPoseFn setControllerPose; //Delegate called when a head pose is received.
		ProcessNewInputFn processNewInput; //Delegate called when new input is received.
		DisconnectFn onDisconnect; //Delegate called when the peer disconnects.
		ReportHandshakeFn reportHandshake;

		const uint32_t& disconnectTimeout;

		CasterContext* casterContext;
		CaptureDelegates captureComponentDelegates;

		ENetHost* host;
		ENetPeer* peer;

		bool receivedHandshake = false;				//Whether we've received the handshake from the client.

		std::vector<avs::uid> nodesEnteredBounds;	//Stores nodes client needs to know have entered streaming bounds.
		std::vector<avs::uid> nodesLeftBounds;		//Stores nodes client needs to know have left streaming bounds.

		struct InputStateAndEvents
		{
			void clear()
			{
				inputState.binaryEventAmount = 0;
				inputState.analogueEventAmount = 0;
				inputState.motionEventAmount = 0;
				newBinaryEvents.clear();
				newAnalogueEvents.clear();
				newMotionEvents.clear();
			}
			avs::InputState inputState;
			//New input events we have received from the client this tick.
			std::vector<avs::InputEventBinary> newBinaryEvents;
			std::vector<avs::InputEventAnalogue> newAnalogueEvents;
			std::vector<avs::InputEventMotion> newMotionEvents;
		};
		InputStateAndEvents newInputStateAndEvents[2]; //Newest input state received from the client.

		void dispatchEvent(const ENetEvent& event);
		void receiveHandshake(const ENetPacket* packet);
		void receiveInput(const ENetPacket* packet);
		void receiveDisplayInfo(const ENetPacket* packet);
		void receiveHeadPose(const ENetPacket* packet);
		void receiveResourceRequest(const ENetPacket* packet);
		void receiveKeyframeRequest(const ENetPacket* packet);
		void receiveClientMessage(const ENetPacket* packet);

		void addNetworkPipelineToAsyncProcessing();
		void removeNetworkPipelineFromAsyncProcessing();
		
		static std::atomic_bool asyncNetworkDataProcessingActive;
		static void processNetworkDataAsync();

		static std::unordered_map<avs::uid, NetworkPipeline*> networkPipelines;
		static std::thread networkThread;
		static std::mutex networkMutex;
		static std::mutex dataMutex;
		static avs::Timestamp lastTickTimestamp;
	};
}