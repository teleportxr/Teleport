// (C) Copyright 2018 Simul.co

#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>

#include "libavstream/common.hpp"
#include "TeleportCore/CommonNetworking.h"
#include <libavstream/libavstream.hpp>
#include <libavstream/genericdecoder.h>
#include <libavstream/genericencoder.h>

#include "TeleportCore/Input.h"
#include "TeleportClient/basic_linear_algebra.h"
#include "ClientPipeline.h"
#include "ClientDeviceState.h"

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
		struct IpPort
		{
			std::string ip;
			int port = 0;
		};
		extern IpPort GetIpPort(const char *ip_port);
		class TabContext;
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
			AWAITING_SETUP,
			HANDSHAKING,
			CONNECTED
		};
		inline const char *StringOf(ConnectionStatus s)
		{
			switch(s)
			{
				case ConnectionStatus::UNCONNECTED		: return "UNCONNECTED";
				case ConnectionStatus::OFFERING			: return "OFFERING";
				case ConnectionStatus::AWAITING_SETUP	: return "AWAITING_SETUP";
				case ConnectionStatus::HANDSHAKING		: return "HANDSHAKING";
				case ConnectionStatus::CONNECTED		: return "CONNECTED";
				default:
				return "INVALID";
			};
		}
		class SessionClient:public avs::GenericTargetInterface
		{
			avs::uid server_uid=0;
			std::string server_ip;
			int server_discovery_port=0;

			teleport::client::ClientPipeline clientPipeline;
			mutable avs::ClientServerMessageStack messageToServerStack;
			// The following MIGHT be moved later to a separate Pipeline class:
			avs::Pipeline messageToServerPipeline;
			avs::GenericEncoder unreliableToServerEncoder;
			ClientServerState clientServerState;
		protected:
			static avs::uid CreateSessionClient(TabContext *tabContext);
			void RequestConnection(const std::string &ip, int port);
			void SetServerIP(std::string);
			void SetServerDiscoveryPort(int);
			friend class TabContext;
			TabContext *tabContext = nullptr;
		public:
			static const std::set<avs::uid> &GetSessionClientIds();
			static std::shared_ptr<teleport::client::SessionClient> GetSessionClient(avs::uid server_uid);
			static void DestroySessionClients();
			// Implementing avs::GenericTargetInterface:
			avs::Result decode(const void* buffer, size_t bufferSizeInBytes) override;
		public:
			SessionClient(avs::uid server_uid, TabContext *tabContext);
			~SessionClient();
			
			ClientServerState &GetClientServerState()
			{
				return clientServerState;
			}
			//! For use only internally or by Renderer for the local session.
			void ApplySetup(const teleport::core::SetupCommand &s);
			bool HandleConnections();
			void SetSessionCommandInterface(SessionCommandInterface*);
			void SetGeometryCache(avs::GeometryCacheBackendInterface* r);
			bool Connect(const char *remoteIP,  uint timeout,avs::uid client_id);
			void Disconnect(uint timeout, bool resetClientID = true);
			void Frame(const avs::DisplayInfo &displayInfo, const avs::Pose &headPose,
					const std::map<avs::uid,avs::PoseDynamic> &controllerPoses, uint64_t originValidCounter,
					const avs::Pose &originPose, const teleport::core::Input& input,
					double time, double deltaTime);
			long long GetServerStartTimeNs() const;
			float GetLatencyMs() const;
			ConnectionStatus GetConnectionStatus() const;
			avs::StreamingConnectionState GetStreamingConnectionState() const;
			bool IsConnecting() const;
			bool IsConnected() const;

			std::string GetServerIP() const;
			
			std::string GetConnectionURL() const;
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
			// Debugging:
			void KillStreaming();
		private:
			void ConfirmOrthogonalStateToClient(uint64_t confNumber);
			void ReceiveCommand(const std::vector<uint8_t> &buffer);
			void ReceiveCommandPacket(const std::vector<uint8_t> &buffer);

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
				size_t totalSize = messageSize + listSize;
				std::vector<uint8_t> buffer(totalSize);
				memcpy(buffer.data(), &msg, messageSize);
				memcpy(buffer.data() + messageSize, appendedList.data(), listSize);

				return SendMessageToServer(buffer.data(), totalSize);
			}
			bool SendMessageToServer(const void* c, size_t sz) const;
			//Tell server we are ready to receive geometry payloads.
			void SendHandshake(const teleport::core::Handshake &handshake, const std::vector<avs::uid>& clientResourceIDs);
			void sendAcknowledgeRemovedNodesMessage(const std::vector<avs::uid> &uids);
	
			void ReceiveHandshakeAcknowledgement(const std::vector<uint8_t> &packet);
			void ReceiveSetupCommand(const std::vector<uint8_t> &packet);
			void ReceiveVideoReconfigureCommand(const std::vector<uint8_t> &packet);
			void ReceiveStageSpaceOriginNodeId(const std::vector<uint8_t> &packet);
			void ReceiveNodeVisibilityUpdate(const std::vector<uint8_t> &packet);
			void ReceiveNodeMovementUpdate(const std::vector<uint8_t> &packet);
			void ReceiveNodeEnabledStateUpdate(const std::vector<uint8_t> &packet);
			void ReceiveNodeHighlightUpdate(const std::vector<uint8_t> &packet);
			void ReceiveNodeAnimationUpdate(const std::vector<uint8_t> &packet);
			void ReceiveNodeAnimationControlUpdate(const std::vector<uint8_t> &packet);
			void ReceiveNodeAnimationSpeedUpdate(const std::vector<uint8_t> &packet);
			void ReceiveSetupLightingCommand(const std::vector<uint8_t> &packet);
			void ReceiveSetupInputsCommand(const std::vector<uint8_t> &packet);
			void ReceiveUpdateNodeStructureCommand(const std::vector<uint8_t> &packet);
			void ReceiveAssignNodePosePathCommand(const std::vector<uint8_t> &packet);
			void ReceivePingForLatencyCommand(const std::vector<uint8_t>& packet);
			void ReceiveTextCommand(const std::vector<uint8_t> &packet);
			static constexpr double RESOURCE_REQUEST_RESEND_TIME = 10.0; //Seconds we wait before resending a resource request.

			uint64_t lastSessionId= 0; //UID of the server session we last connected to.

			SessionCommandInterface* mCommandInterface=nullptr;

			avs::GeometryCacheBackendInterface* geometryCache = nullptr;


			/// Requests the session client has discovered need to be made.
			std::vector<avs::uid> mQueuedResourceRequests;			
			std::map<avs::uid, double> mSentResourceRequests;	/// <ID of requested resource, time we sent the request> Resource requests we have received, but have yet to receive confirmation of their receipt.
			std::vector<avs::uid> mReceivedNodes;				/// Nodes that have entered bounds, are about to be drawn, and need to be confirmed to the server.
			std::vector<avs::uid> mLostNodes;					/// Node that have left bounds, are about to be hidden, and need to be confirmed to the server.

			uint64_t clientID=0;
			double time=0.0;
			float latency_milliseconds = 0.0;
			// State received from server.
			teleport::core::SetupCommand setupCommand;
			teleport::core::SetupLightingCommand setupLightingCommand;
			std::vector<teleport::core::InputDefinition> inputDefinitions;
			std::map<avs::uid, std::string> nodePosePaths;
			//! Reset the session state when connecting to a new server, or when reconnecting without preserving the session:
			void ResetSessionState();

			std::string remoteIP;
			std::string connected_url;
			double mTimeSinceLastServerComm = 0;

			ConnectionStatus connectionStatus = ConnectionStatus::UNCONNECTED;
		};
	}
}