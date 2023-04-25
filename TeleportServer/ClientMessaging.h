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
#include <libavstream/genericencoder.h>

typedef void(TELEPORT_STDCALL* SetHeadPoseFn) (avs::uid uid, const avs::Pose*);
typedef void(TELEPORT_STDCALL* SetControllerPoseFn) (avs::uid uid, int index, const avs::PoseDynamic*);
typedef void(TELEPORT_STDCALL* ProcessNewInputStateFn) (avs::uid uid, const teleport::core::InputState*, const uint8_t**, const float**);
typedef void(TELEPORT_STDCALL* ProcessNewInputEventsFn) (avs::uid uid, uint16_t,uint16_t,uint16_t,const avs::InputEventBinary**, const avs::InputEventAnalogue**, const avs::InputEventMotion**);
typedef void(TELEPORT_STDCALL* DisconnectFn) (avs::uid uid);
typedef void(TELEPORT_STDCALL* ReportHandshakeFn) (avs::uid clientID,const teleport::core::Handshake *h);

namespace teleport
{
	namespace server
	{
		class SignalingService;
		class ClientManager;
		//! Per-client messaging handler.
		class ClientMessaging:public avs::GenericTargetInterface
		{
			bool stopped = false;
			mutable avs::ClientServerMessageStack commandStack;
		public:
			ClientMessaging(const struct ServerSettings* settings,
				SignalingService &discoveryService,
				SetHeadPoseFn setHeadPose,
				SetControllerPoseFn setControllerPose,
				ProcessNewInputStateFn processNewInputState,
				ProcessNewInputEventsFn processNewInputEvents,
				DisconnectFn onDisconnect,
				uint32_t disconnectTimeout,
				ReportHandshakeFn reportHandshakeFn,
				ClientManager* clientManager);

			virtual ~ClientMessaging();


			avs::ConnectionState getConnectionState() const;

			bool isInitialised() const;
			void initialize( CaptureDelegates captureDelegates);
			void unInitialise();

			bool startSession(avs::uid clientID, std::string clientIP);

			void ensureStreamingPipeline();
			void stopSession();
			bool isStopped() const;
			bool isStartingSession() { return startingSession; }
			void tick(float deltaTime);
			void handleEvents(float deltaTime);
			void Disconnect();

			void nodeEnteredBounds(avs::uid nodeID);
			void nodeLeftBounds(avs::uid nodeID);
			void updateNodeMovement(const std::vector<teleport::core::MovementUpdate>& updateList);
			void updateNodeEnabledState(const std::vector<teleport::core::NodeUpdateEnabledState>& updateList);
			void setNodeHighlighted(avs::uid nodeID, bool isHighlighted);
			void reparentNode(avs::uid nodeID, avs::uid newParentID, avs::Pose relPose);
			void setNodePosePath(avs::uid nodeID, const std::string& regexPosePath);
			void updateNodeAnimation(teleport::core::ApplyAnimation update);
			void updateNodeAnimationControl(teleport::core::NodeUpdateAnimationControl update);
			void updateNodeRenderState(avs::uid nodeID, avs::NodeRenderState update);
			void setNodeAnimationSpeed(avs::uid nodeID, avs::uid animationID, float speed);

			bool hasPeer() const;
			bool hasReceivedHandshake() const;

			bool setOrigin(uint64_t valid_counter, avs::uid originNode);
			template<typename C> bool sendCommand(const C& command) const
			{
				if (!peer)
				{
					TELEPORT_CERR << "Failed to send command with type: " << static_cast<int>(command.commandPayloadType) << "! ClientMessaging has no peer!\n";
					return false;
				}

				size_t commandSize = sizeof(C);
				ENetPacket* packet = enet_packet_create(&command, commandSize, ENET_PACKET_FLAG_RELIABLE);

				if (!packet)
				{
					TELEPORT_CERR << "Failed to send command with type: " << static_cast<int>(command.commandPayloadType) << "! Failed to create packet!\n";
					return false;
				}

				return enet_peer_send(peer, static_cast<enet_uint8>(teleport::core::RemotePlaySessionChannel::RPCH_Control), packet) == 0;
			}
			template<typename C> bool sendCommand2(const C& command) const
			{
				return SendCommand(&command,sizeof(command));
			}
			bool SendCommand(const void* c, size_t sz) const;

			uint16_t getServerPort() const;

			uint16_t getStreamingPort() const;

			template<typename C, typename T> bool sendCommand(const C& command, const std::vector<T>& appendedList) const
			{
				if (!peer)
				{
					TELEPORT_CERR << "Failed to send command with type: " << static_cast<uint8_t>(command.commandPayloadType) << "! ClientMessaging has no peer!\n";
					return false;
				}

				size_t commandSize = sizeof(C);
				size_t listSize = sizeof(T) * appendedList.size();

				ENetPacket* packet = enet_packet_create(&command, commandSize, ENET_PACKET_FLAG_RELIABLE);

				if (!packet)
				{
					TELEPORT_CERR << "Failed to send command with type: " << static_cast<uint8_t>(command.commandPayloadType) << "! Failed to create packet!\n";
					return false;
				}

				//Copy list into packet.
				enet_packet_resize(packet, commandSize + listSize);
				memcpy(packet->data + commandSize, appendedList.data(), listSize);

				return enet_peer_send(peer, static_cast<enet_uint8>(teleport::core::RemotePlaySessionChannel::RPCH_Control), packet) == 0;
			}

			template<typename C, typename T> bool sendCommand2(const C& command, const std::vector<T>& appendedList) const
			{
				size_t commandSize = sizeof(C);
				size_t listSize = sizeof(T) * appendedList.size();
				size_t totalSize = commandSize + listSize;
				std::vector<uint8_t> buffer(totalSize);
				memcpy(buffer.data() , &command, commandSize);
				memcpy(buffer.data()+ commandSize, appendedList.data(), listSize);

				return SendCommand(buffer.data(), totalSize) ;
			}
			template <> bool sendCommand<teleport::core::SetupInputsCommand, teleport::core::InputDefinition>(const teleport::core::SetupInputsCommand& command, const std::vector<teleport::core::InputDefinition>& appendedInputDefinitions) const
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
				size_t listSize = appendedInputDefinitions.size() * (sizeof(avs::InputId) + sizeof(avs::InputType));
				for (const auto& d : appendedInputDefinitions)
				{
					// a uint16 to store the path length.
					listSize += sizeof(uint16_t);
					// and however many chars in the path.
					listSize += sizeof(char) * d.regexPath.length();
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
					defPacket.pathLength = (uint16_t)d.regexPath.length();
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
			bool video_encoder_initialized = false;
			ClientNetworkContext* getClientNetworkContext()
			{
				return &clientNetworkContext;
			}

			bool timedOutStartingSession() const;

			void ConfirmSessionStarted();

			GeometryStreamingService& GetGeometryStreamingService()
			{
				return geometryStreamingService;
			}

			// Generic target
			avs::Result decode(const void* buffer, size_t bufferSizeInBytes) override;
		private:

			// The following MIGHT be moved later to a separate Pipeline class:
			avs::Pipeline commandPipeline;
			avs::GenericEncoder commandEncoder;
			// A pipeline on the main thread to receive messages.
			avs::GenericDecoder MessageDecoder;
			avs::Pipeline messagePipeline;

			friend class ClientManager;
			void receive(const ENetEvent& event);
			void receiveHandshake(const ENetPacket* packet);
			void receiveInputStates(const ENetPacket* packet);
			void receiveInputEvents(const ENetPacket* packet); 
			void receiveDisplayInfo(const ENetPacket* packet);
			void receiveHeadPose(const ENetPacket* packet);
			void receiveResourceRequest(const ENetPacket* packet);
			void receiveKeyframeRequest(const ENetPacket* packet);
			void receiveClientMessage(const ENetPacket* packet);
			void receiveStreamingControl(const ENetPacket* packet);

			void sendStreamingControlMessage(const std::string& msg);
			avs::ThreadSafeQueue<ENetEvent> eventQueue;
			teleport::core::Handshake handshake;
			static bool asyncNetworkDataProcessingFailed;
			avs::uid clientID=0;
			std::string clientIP;
			bool initialized = false;
			bool startingSession=false;
			float timeStartingSession=0.0f;
			float timeSinceLastClientComm=0.0f;
			const ServerSettings* settings=nullptr;
			SignalingService &discoveryService;
			PluginGeometryStreamingService geometryStreamingService;
			ClientManager* clientManager;
			SetHeadPoseFn setHeadPose; //Delegate called when a head pose is received.
			SetControllerPoseFn setControllerPose; //Delegate called when a head pose is received.
			ProcessNewInputStateFn processNewInputState; //Delegate called when new input is received.
			ProcessNewInputEventsFn processNewInputEvents; //Delegate called when new input is received.
			DisconnectFn onDisconnect; //Delegate called when the peer disconnects.
			ReportHandshakeFn reportHandshake;

			uint32_t disconnectTimeout=0;
			uint16_t streamingPort=0;

			ClientNetworkContext clientNetworkContext;

			CaptureDelegates captureComponentDelegates;

			ENetPeer* peer = nullptr;

			std::atomic_bool receivedHandshake = false;				//Whether we've received the handshake from the client.

			std::vector<avs::uid> nodesEnteredBounds;	//Stores nodes client needs to know have entered streaming bounds.
			std::vector<avs::uid> nodesLeftBounds;		//Stores nodes client needs to know have left streaming bounds.

			core::Input latestInputStateAndEvents; //Latest input state received from the client.

			// Seconds
			static constexpr float startSessionTimeout = 3;
		};
	}
}