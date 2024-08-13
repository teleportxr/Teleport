#pragma once

#include <atomic>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

#include "libavstream/common_input.h"
#include "CaptureDelegates.h"
#include "ServerSettings.h"
#include "GeometryStreamingService.h"
#include "VideoEncodePipeline.h"
#include "TeleportCore/Input.h"
#include "Export.h"
#include <libavstream/genericencoder.h>
#include "Exports.h"

namespace teleport
{
	namespace server
	{
		class SignalingService;
		class ClientManager;

		struct OrthogonalNodeState
		{
			int64_t serverTimeSentUs = 0;
			uint64_t confirmationNumber=0;
		};
		// A representation of the node state that has been sent to a specific client.
		// For each relevant command payload type, we store the last "confirmation number" that was sent to the client.
		// A confirmation number corresponds to a specific state, so if the state changes, the conf number changes,
		// But if we re-send the command, it will have the same conf number.
		struct OrthogonalNodeStateMap
		{
			std::map<core::CommandPayloadType, OrthogonalNodeState> unconfirmedStates;
		};
		/// Per-client messaging handler.
		class TELEPORT_SERVER_API ClientMessaging:public avs::GenericTargetInterface
		{
			bool stopped = false;
			mutable avs::ClientServerMessageStack commandStack;
		public:
			ClientMessaging(SignalingService &signalingService,
				SetHeadPoseFn setHeadPose,
				SetControllerPoseFn setControllerPose,
				ProcessNewInputStateFn processNewInputState,
				ProcessNewInputEventsFn processNewInputEvents,
				DisconnectFn onDisconnect,
				uint32_t disconnectTimeout,
				ReportHandshakeFn reportHandshakeFn, avs::uid clid
				);

			virtual ~ClientMessaging();
			//! Get current server-time in nanoseconds (i.e. time since server started).
			int64_t GetServerTimeUs() const;

			avs::StreamingConnectionState getStreamingState() const;
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
			
			bool hasOrigin() const
			{
				return(currentOriginState.originClientHas!=0&&currentOriginState.sent);
			}
			void setHasOrigin(bool o)
			{
				currentOriginState.sent=o;
				if(!o)
					currentOriginState.acknowledged=false;
				currentOriginState.originClientHas = 0;
			}

			bool setOrigin(avs::uid uid);
			avs::uid getOrigin() const;

			void sendSetupCommand(const teleport::core::SetupCommand& setupCommand
				, const teleport::core::SetupLightingCommand setupLightingCommand, const std::vector<avs::uid>& global_illumination_texture_uids
				, const teleport::core::SetupInputsCommand& setupInputsCommand, const std::vector<teleport::core::InputDefinition>& inputDefinitions);

			void sendReconfigureVideoCommand(const core::ReconfigureVideoCommand& cmd);
			void sendSetupLightingCommand(const teleport::core::SetupLightingCommand setupLightingCommand, const std::vector<avs::uid>& global_illumination_texture_uids);

			void updateNodeMovement(const std::vector<teleport::core::MovementUpdate>& updateList);
			void updateNodeEnabledState(const std::vector<teleport::core::NodeUpdateEnabledState>& updateList);
			void setNodeHighlighted(avs::uid nodeID, bool isHighlighted);
			void reparentNode(const teleport::core::UpdateNodeStructureCommand& cmd);
			void setNodePosePath(avs::uid nodeID, const std::string& regexPosePath);
			void updateNodeAnimation(teleport::core::ApplyAnimation update);
			void updateNodeRenderState(avs::uid nodeID, avs::NodeRenderState update);
			void setNodeAnimationSpeed(avs::uid nodeID, avs::uid animationID, float speed);

			void pingForLatency();

			bool hasReceivedHandshake() const;

			std::string getClientIP() const
			{
				return clientIP;
			};

			avs::uid getClientID() const
			{
				return clientID;
			}

			// Same as client ip.
			std::string getPeerIP() const;

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
			const avs::DisplayInfo& getDisplayInfo() const;
			std::set<uint64_t> GetAndResetConfirmationsReceived()
			{
				return std::move(confirmationsReceived);
			}
			const teleport::core::SetupCommand &GetLastSetupCommand() const
			{
				return lastSetupCommand;
			}
		private:
			uint64_t next_ack_id=0;
			std::set<uint64_t> confirmationsReceived;
			float timeSinceLastGeometryStream = 0;
			int framesSinceLastPing = 99;
			// The following MIGHT be moved later to a separate Pipeline class:
			avs::Pipeline commandPipeline;
			avs::GenericEncoder commandEncoder;
			// A pipeline on the main thread to receive messages.
			avs::GenericDecoder MessageDecoder;
			avs::Pipeline messagePipeline;

			friend class ClientManager;


			template<typename C> bool sendSignalingCommand(const C& command)
			{
				size_t commandSize = sizeof(C);
				std::vector<uint8_t> bin(commandSize);
				memcpy(bin.data(), &command, commandSize);
				return SendSignalingCommand(std::move(bin));
			}
			template<typename C> size_t sendCommand(const C& command) const
			{
				return SendCommand(&command, sizeof(command));
			}
			size_t SendCommand(const void* c, size_t sz) const;
			bool SendSignalingCommand(std::vector<uint8_t>&& bin);
			void Warn(const char *w) const;
			template<typename C, typename T> size_t sendCommand(const C& command, const std::vector<T>& appendedList) const
			{
				size_t commandSize = sizeof(C);
				size_t listSize = sizeof(T) * appendedList.size();
				size_t totalSize = commandSize + listSize;
				std::vector<uint8_t> buffer(totalSize);
				memcpy(buffer.data(), &command, commandSize);
				memcpy(buffer.data() + commandSize, appendedList.data(), listSize);

				return SendCommand(buffer.data(), totalSize);
			}
			template<typename C, typename T> bool sendSignalingCommand(const C& command, const std::vector<T>& appendedList)
			{
				size_t commandSize = sizeof(C);
				size_t listSize = sizeof(T) * appendedList.size();
				std::vector<uint8_t> bin(commandSize + listSize);
				memcpy(bin.data(), &command, commandSize);
				memcpy(bin.data() + commandSize, appendedList.data(), listSize);

				return SendSignalingCommand(std::move(bin));
			}
			template <> bool sendSignalingCommand<teleport::core::SetupInputsCommand, teleport::core::InputDefinition>(const teleport::core::SetupInputsCommand& command, const std::vector<teleport::core::InputDefinition>& appendedInputDefinitions)
			{
				if (command.commandPayloadType != teleport::core::CommandPayloadType::SetupInputs)
				{
					Warn("Invalid command!\n");
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
						Warn("Input path too long!\n");
						return false;
					}
				}
				std::vector<uint8_t> bin(commandSize + listSize);
				memcpy(bin.data(), &command, commandSize);
				unsigned char* data_ptr = bin.data() + commandSize;
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
				if (bin.data() + commandSize + listSize != data_ptr)
				{
					Warn("Failed to send command due to packet size discrepancy\n");
					return false;
				}

				return SendSignalingCommand(std::move(bin));
			}

			void receiveSignaling(const std::vector<uint8_t> &bin);
			void receiveHandshake(const std::vector<uint8_t> &bin);
			void receiveInputStates(const std::vector<uint8_t> &bin);
			void receiveInputEvents(const std::vector<uint8_t> &bin); 
			void receiveDisplayInfo(const std::vector<uint8_t> &bin);
			void receiveKeyframeRequest(const std::vector<uint8_t> &bin);
			void receiveClientMessage(const std::vector<uint8_t> &bin);
			void receiveStreamingControl(const std::vector<uint8_t> &bin);
			void receivePongForLatency(const std::vector<uint8_t>& packet);
			void receiveAcknowledgement(const std::vector<uint8_t> &packet);

			void sendStreamingControlMessage(const std::string& msg);
			teleport::core::Handshake handshake;
			static bool asyncNetworkDataProcessingFailed;
			avs::uid clientID=0;
			std::string clientIP;
			avs::DisplayInfo displayInfo;
			struct OriginState
			{
				bool sent=false;
				avs::uid originClientHas=0;
				uint64_t ack_id=0;
				bool acknowledged=false;
				int64_t serverTimeSentUs=0;
				uint64_t valid_counter=0;
			};
			OriginState currentOriginState;
			bool initialized = false;
			bool startingSession=false;
			float timeStartingSession=0.0f;
			float timeSinceLastClientComm=0.0f;
			float client_to_server_latency_milliseconds = 0.0f;
			float server_to_client_latency_milliseconds = 0.0f;
			SignalingService &signalingService;
			GeometryStreamingService geometryStreamingService;
			SetHeadPoseFn setHeadPose=nullptr;								// Delegate called when a head pose is received.
			SetControllerPoseFn setControllerPose = nullptr;				// Delegate called when a head pose is received.
			ProcessNewInputStateFn processNewInputState = nullptr;			// Delegate called when new input is received.
			ProcessNewInputEventsFn processNewInputEvents = nullptr;		// Delegate called when new input is received.
			DisconnectFn onDisconnect = nullptr;							// Delegate called when the peer disconnects.
			ReportHandshakeFn reportHandshake = nullptr;

			uint32_t disconnectTimeout=0;

			ClientNetworkContext clientNetworkContext;

			CaptureDelegates captureComponentDelegates;

			std::atomic_bool receivedHandshake = false;				//Whether we've received the handshake from the client.

			core::Input latestInputStateAndEvents; //Latest input state received from the client.

			// Seconds
			static constexpr float startSessionTimeout = 3;
			mutable core::ClientNetworkState clientNetworkState;
			teleport::core::SetupCommand lastSetupCommand;
		};
	}
}