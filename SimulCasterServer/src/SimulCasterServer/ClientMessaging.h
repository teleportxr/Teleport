#pragma once

#include <functional>
#include <vector>
#include <thread>
#include <atomic>
#include <mutex>

#include "CaptureDelegates.h"
#include "CasterSettings.h"
#include "GeometryStreamingService.h"
#include "VideoEncodePipeline.h"


typedef void(__stdcall* ProcessNewInputFn) (avs::uid uid, const avs::InputState*, const avs::InputEvent*);

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
						std::function<void(avs::uid,const avs::Pose*)> setHeadPose,
						std::function<void(avs::uid,int index,const avs::Pose*)> setControllerPose,
						std::function<void(avs::uid,const avs::InputState*,const avs::InputEvent*)> processNewInput,
						std::function<void(void)> onDisconnect,
						const int32_t& disconnectTimeout);
		
		virtual ~ClientMessaging();

		bool isInitialised() const;
		void initialise(CasterContext* context, CaptureDelegates captureDelegates);

		bool startSession(avs::uid u, int32_t listenPort);
		void stopSession();

		void tick(float deltaTime);
		void handleEvents(float deltaTime);

		void actorEnteredBounds(avs::uid actorID);
		void actorLeftBounds(avs::uid actorID);
		void updateActorMovement(std::vector<avs::MovementUpdate>& updateList);

		bool hasHost() const;
		bool hasPeer() const;
		bool hasReceivedHandshake() const;

		bool setPosition(const avs::vec3 &pos);

		bool sendCommand(const avs::Command& avsCommand) const;
		template<typename T> bool sendCommand(const avs::Command& avsCommand, std::vector<T>& appendedList) const
		{
			assert(peer);

			size_t commandSize = avs::GetCommandSize(avsCommand.commandPayloadType);
			size_t listSize = sizeof(T) * appendedList.size();

			ENetPacket* packet = enet_packet_create(&avsCommand, commandSize, ENET_PACKET_FLAG_RELIABLE);
			assert(packet);

			//Copy list into packet.
			enet_packet_resize(packet, commandSize + listSize);
			memcpy(packet->data + commandSize, appendedList.data(), listSize);
			
			return enet_peer_send(peer, static_cast<enet_uint8>(avs::RemotePlaySessionChannel::RPCH_Control), packet) == 0;
		}

		std::string getClientIP() const;
		uint16_t getClientPort() const;
		uint16_t getServerPort() const;
		float getBandWidthKPS() const;

		static void startAsyncNetworkDataProcessing();
		static void stopAsyncNetworkDataProcessing(bool killThread = true);

	private:
		static bool asyncNetworkDataProcessingFailed;
		avs::uid clientID;
		bool initialized=false;
		const CasterSettings* settings;
		std::shared_ptr<DiscoveryService> discoveryService;
		std::shared_ptr<GeometryStreamingService> geometryStreamingService;

		std::function<void(avs::uid,const avs::Pose*)> setHeadPose;			//Delegate called when a head pose is received.
		std::function<void(avs::uid,int index,const avs::Pose*)> setControllerPose;			//Delegate called when a head pose is received.
		std::function<void(avs::uid,const avs::InputState*,const avs::InputEvent*)> processNewInput;	//Delegate called when new input is received.
		std::function<void(void)> onDisconnect; //Delegate called when the peer disconnects.

		const int32_t& disconnectTimeout;

		CasterContext* casterContext;
		CaptureDelegates captureComponentDelegates;

		ENetHost* host;
		ENetPeer* peer;

		bool receivedHandshake = false; //Whether we've received the handshake from the client.

		std::vector<avs::uid> actorsEnteredBounds; //Stores actors client needs to know have entered streaming bounds.
		std::vector<avs::uid> actorsLeftBounds; //Stores actors client needs to know have left streaming bounds.

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
	};
}