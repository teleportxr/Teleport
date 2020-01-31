#pragma once

#include <functional>
#include <vector>

#include "CaptureDelegates.h"
#include "CasterSettings.h"
#include "GeometryStreamingService.h"

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
		ClientMessaging(const struct CasterSettings* settings, std::shared_ptr<DiscoveryService> discoveryService, std::shared_ptr<GeometryStreamingService> geometryStreamingService,
						std::function<void(const avs::HeadPose*)> setHeadPose, std::function<void(const avs::InputState*)> processNewInput, std::function<void(void)> onDisconnect,
						const int32_t& disconnectTimeout);

		void initialise(CasterContext* context, CaptureDelegates captureDelegates);

		bool startSession(int32_t listenPort);
		void stopSession();

		void tick(float deltaTime);
		void handleEvents();

		void actorEnteredBounds(avs::uid actorID);
		void actorLeftBounds(avs::uid actorID);

		bool hasHost() const;
		bool hasPeer() const;

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

		bool sendSetupCommand(avs::SetupCommand&& setupCommand);

		std::string getClientIP() const;
		uint16_t getClientPort() const;
		uint16_t getServerPort() const;
	private:
		const CasterSettings* settings;
		std::shared_ptr<DiscoveryService> discoveryService;
		std::shared_ptr<GeometryStreamingService> geometryStreamingService;

		std::function<void(const avs::HeadPose*)> setHeadPose; //Delegate called when a head pose is received.
		std::function<void(const avs::InputState*)> processNewInput; //Delegate called when new input is received.
		std::function<void(void)> onDisconnect; //Delegate called when the peer disconnects.

		const int32_t& disconnectTimeout;

		CasterContext* casterContext;
		CaptureDelegates captureComponentDelegates;

		ENetHost* host;
		ENetPeer* peer;

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
	};
}