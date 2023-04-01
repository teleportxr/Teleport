// (C) Copyright 2018 Simul.co

#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>

#include <enet/enet.h>
#include "libavstream/common.hpp"
#include "TeleportCore/CommonNetworking.h"
#include <libavstream/libavstream.hpp>
#include <libavstream/genericdecoder.h>
#include <libavstream/genericencoder.h>

#include "TeleportCore/Input.h"
#include "TeleportClient/basic_linear_algebra.h"
#include "ClientPipeline.h"

typedef unsigned int uint;

namespace avs
{
	class GeometryTargetBackendInterface;
	class GeometryCacheBackendInterface;
}

namespace teleport
{
	namespace client
	{
		class SessionCommandInterface
		{
		public:
			virtual bool OnSetupCommandReceived(const char* server_ip, const teleport::core::SetupCommand& setupCommand, teleport::core::Handshake& handshake) = 0;
			virtual void OnVideoStreamClosed() = 0;

			virtual void OnReconfigureVideo(const teleport::core::ReconfigureVideoCommand& reconfigureVideoCommand) = 0;

			virtual bool OnNodeEnteredBounds(avs::uid node_uid) = 0;
			virtual bool OnNodeLeftBounds(avs::uid node_uid) = 0;
			virtual void OnInputsSetupChanged(const std::vector<teleport::core::InputDefinition>& inputDefinitions) =0;
			virtual void UpdateNodeStructure(const teleport::core::UpdateNodeStructureCommand& ) =0;
			virtual void AssignNodePosePath(const teleport::core::AssignNodePosePathCommand &,const std::string &)=0;
			virtual void SetOrigin(unsigned long long ctr,avs::uid origin_node_uid)=0;

			virtual std::vector<avs::uid> GetGeometryResources() = 0;
			virtual void ClearGeometryResources() = 0;

			virtual void SetVisibleNodes(const std::vector<avs::uid>& visibleNodes) = 0;
			virtual void UpdateNodeMovement(const std::vector<teleport::core::MovementUpdate>& updateList) = 0;
			virtual void UpdateNodeEnabledState(const std::vector<teleport::core::NodeUpdateEnabledState>& updateList) = 0;
			virtual void SetNodeHighlighted(avs::uid nodeID, bool isHighlighted) = 0;
			virtual void UpdateNodeAnimation(const teleport::core::ApplyAnimation& animationUpdate) = 0;
			virtual void UpdateNodeAnimationControl(const teleport::core::NodeUpdateAnimationControl& animationControlUpdate) = 0;
			virtual void SetNodeAnimationSpeed(avs::uid nodeID, avs::uid animationID, float speed) = 0;
			virtual void OnStreamingControlMessage(const std::string& str) = 0;
		};
		
		enum class WebspaceLocation : uint8_t
		{
			UNKNOWN,
			LOBBY,
			SERVER,

			HOME = LOBBY,
		};
		enum class ConnectionStatus : uint8_t
		{
			UNCONNECTED,
			OFFERING,
			HANDSHAKING,
			CONNECTED
		};
		class SessionClient:public avs::GenericTargetInterface
		{
			avs::uid server_uid=0;
			std::string server_ip;
			int server_discovery_port=0;

			teleport::client::ClientPipeline clientPipeline;
			mutable avs::ClientServerMessageStack messageToServerStack;
			// The following MIGHT be moved later to a separate Pipeline class:
			avs::Pipeline messageToServerPipeline;
			avs::GenericEncoder messageToServerEncoder;
		public:
			static std::shared_ptr<teleport::client::SessionClient> GetSessionClient(avs::uid server_uid);
			static void DestroySessionClients();
			static void ConnectButtonHandler(avs::uid server_uid,const std::string& url);
			static void CancelConnectButtonHandler(avs::uid server_uid);
			// Implementing avs::GenericTargetInterface:
			avs::Result decode(const void* buffer, size_t bufferSizeInBytes) override;
		public:
			SessionClient(avs::uid server_uid);
			~SessionClient();
			
			void RequestConnection(const std::string &ip,int port);
			bool HandleConnections();
			void SetSessionCommandInterface(SessionCommandInterface*);
			void SetGeometryCache(avs::GeometryCacheBackendInterface* r);
			bool Connect(const char *remoteIP, uint16_t remotePort, uint timeout,avs::uid client_id);
			bool Connect(const ENetAddress &remote, uint timeout,avs::uid client_id);
			void Disconnect(uint timeout, bool resetClientID = true);
			void SetPeerTimeout(uint timeout);
			void Frame(const avs::DisplayInfo &displayInfo, const avs::Pose &headPose,
					const std::map<avs::uid,avs::PoseDynamic> &controllerPoses, uint64_t originValidCounter,
					const avs::Pose &originPose, const teleport::core::Input& input,
					double time, double deltaTime);
					
			ConnectionStatus GetConnectionStatus() const;
			bool IsConnecting() const;
			bool IsConnected() const;

			std::string GetServerIP() const;
			
			int GetServerDiscoveryPort() const;
			void SetServerIP(std::string) ;
			void SetServerDiscoveryPort(int) ;
			int GetPort() const;

			unsigned long long receivedInitialPos = 0;

			uint64_t GetClientID() const
			{
				return clientID;
			}
				
			const std::map<avs::uid, double> &GetSentResourceRequests() const{
				return mSentResourceRequests;
			};

			void SendStreamingControlMessage(const std::string& str);
			const std::vector<teleport::core::InputDefinition>& GetInputDefinitions() const
			{
				return inputDefinitions;
			}
			void SetInputDefinitions(const std::vector<teleport::core::InputDefinition>& d)
			{
				inputDefinitions=d;
			}
			const std::map<avs::uid, std::string>& GetNodePosePaths() const
			{
				return nodePosePaths;
			}
			const teleport::core::SetupCommand& GetSetupCommand() const
			{
				return setupCommand;
			}
			const teleport::core::SetupLightingCommand& GetSetupLightingCommand() const
			{
				return setupLightingCommand;
			}
			teleport::client::ClientPipeline &GetClientPipeline()
			{
				return clientPipeline;
			}
		private:
			template<typename MessageType> void SendClientMessage(const MessageType &message)
			{
				size_t messageSize = sizeof(MessageType);
				ENetPacket* packet = enet_packet_create(&message, messageSize, ENET_PACKET_FLAG_UNRELIABLE_FRAGMENT);
				enet_peer_send(mServerPeer, static_cast<enet_uint8>(teleport::core::RemotePlaySessionChannel::RPCH_ClientMessage), packet);
			}
			void DispatchEvent(const ENetEvent& event);
			void ReceiveCommandPacket(ENetPacket* packet);

			void SendDisplayInfo(const avs::DisplayInfo& displayInfo);
			void SendNodePoses(const avs::Pose& headPose,const std::map<avs::uid,avs::PoseDynamic> poses);
			void SendInput(const teleport::core::Input& input);
			void SendResourceRequests();
			void SendReceivedResources();
			void SendNodeUpdates();
			void SendKeyframeRequest();

			// WebRTC:
			template<typename C> bool sendMessageToServer(const C& command) const
			{
				return SendMessageToServer(&command, sizeof(command));
			}
			template<typename M, typename T> bool sendMessageToServer(const M& msg, const std::vector<T>& appendedList) const
			{
				size_t messageSize = sizeof(M);
				size_t listSize = sizeof(T) * appendedList.size();
				size_t totalSize = commandSize + listSize;
				std::vector<uint8_t> buffer(totalSize);
				memcpy(buffer.data(), &command, commandSize);
				memcpy(buffer.data() + commandSize, appendedList.data(), listSize);

				return SendMessageToServer(buffer.data(), totalSize);
			}
			bool SendMessageToServer(const void* c, size_t sz) const;
			//Tell server we are ready to receive geometry payloads.
			void SendHandshake(const teleport::core::Handshake &handshake, const std::vector<avs::uid>& clientResourceIDs);
			void sendAcknowledgeRemovedNodesMessage(const std::vector<avs::uid> &uids);
	
			void ReceiveHandshakeAcknowledgement(const ENetPacket* packet);
			void ReceiveSetupCommand(const ENetPacket* packet);
			void ReceiveVideoReconfigureCommand(const ENetPacket* packet);
			void ReceiveStageSpaceOriginNodeId(const ENetPacket* packet);
			void ReceiveNodeVisibilityUpdate(const ENetPacket* packet);
			void ReceiveNodeMovementUpdate(const ENetPacket* packet);
			void ReceiveNodeEnabledStateUpdate(const ENetPacket* packet);
			void ReceiveNodeHighlightUpdate(const ENetPacket* packet);
			void ReceiveNodeAnimationUpdate(const ENetPacket* packet);
			void ReceiveNodeAnimationControlUpdate(const ENetPacket* packet);
			void ReceiveNodeAnimationSpeedUpdate(const ENetPacket* packet);
			void ReceiveSetupLightingCommand(const ENetPacket* packet);
			void ReceiveSetupInputsCommand(const ENetPacket* packet);
			void ReceiveUpdateNodeStructureCommand(const ENetPacket* packet);
			void ReceiveAssignNodePosePathCommand(const ENetPacket* packet);
			void ReceiveTextCommand(const ENetPacket* packet);
			static constexpr double RESOURCE_REQUEST_RESEND_TIME = 10.0; //Seconds we wait before resending a resource request.

			avs::uid lastServerID = 0; //UID of the server we last connected to.

			SessionCommandInterface* mCommandInterface=nullptr;

			avs::GeometryCacheBackendInterface* geometryCache = nullptr;

			ENetHost* mClientHost = nullptr;
			ENetPeer* mServerPeer = nullptr;
			ENetAddress mServerEndpoint{};

			/// Requests the session client has discovered need to be made.
			std::vector<avs::uid> mQueuedResourceRequests;			
			std::map<avs::uid, double> mSentResourceRequests;	/// <ID of requested resource, time we sent the request> Resource requests we have received, but have yet to receive confirmation of their receipt.
			std::vector<avs::uid> mReceivedNodes;				/// Nodes that have entered bounds, are about to be drawn, and need to be confirmed to the server.
			std::vector<avs::uid> mLostNodes;					/// Node that have left bounds, are about to be hidden, and need to be confirmed to the server.

			uint64_t clientID=0;

			double time=0.0;

			// State received from server.
			teleport::core::SetupCommand setupCommand;
			teleport::core::SetupLightingCommand setupLightingCommand;
			std::vector<teleport::core::InputDefinition> inputDefinitions;
			std::map<avs::uid, std::string> nodePosePaths;
			//! Reset the session state when connecting to a new server, or when reconnecting without preserving the session:
			void ResetSessionState();

			std::string remoteIP;
			double mTimeSinceLastServerComm = 0;

			ConnectionStatus connectionStatus = ConnectionStatus::UNCONNECTED;
		};
	}
}