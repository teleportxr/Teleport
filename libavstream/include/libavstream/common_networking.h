#pragma once

#include <cstdint>
#include <iostream>

#include "common.hpp"
#include "common_maths.h"
#include "common_input.h"

namespace avs
{
#ifdef _MSC_VER
#pragma pack(push, 1)
#endif
	enum class BackgroundMode : uint8_t
	{
		NONE = 0, COLOUR, TEXTURE, VIDEO
	};
	enum class RemotePlaySessionChannel : unsigned char //enet_uint8
	{
		RPCH_Handshake = 0,
		RPCH_Control = 1,
		RPCH_DisplayInfo = 2,
		RPCH_HeadPose = 3,
		RPCH_ResourceRequest = 4,
		RPCH_KeyframeRequest = 5,
		RPCH_ClientMessage = 6,
		RPCH_Origin = 7,
		RPCH_NumChannels
	};

	enum class ControlModel : uint32_t
	{
		NONE,
		CLIENT_ORIGIN_SERVER_GRAVITY,
		SERVER_ORIGIN_CLIENT_LOCAL
	};

	enum class NodeStatus : uint8_t
	{
		Unknown = 0,
		Drawn,
		WantToRelease,
		Released
	};
	
	//! The payload type, or how to interpret the server's message.
	enum class CommandPayloadType : uint8_t
	{
		Invalid,
		Shutdown,
		Setup,
		AcknowledgeHandshake,
		ReconfigureVideo,
		SetPosition,
		NodeVisibility,
		UpdateNodeMovement,
		UpdateNodeEnabledState,
		SetNodeHighlighted,
		UpdateNodeAnimation,
		UpdateNodeAnimationControl,
		SetNodeAnimationSpeed,
		SetupLighting,
		UpdateNodeStructure,
		UpdateNodeSubtype,
		SetupInputs,
	};

	//! The payload type, or how to interpret the client's message.
	enum class ClientMessagePayloadType : uint8_t
	{
		Invalid,
		NodeStatus,
		ReceivedResources,
		ControllerPoses,
		OriginPose,
	};
	
	//! The response payload sent by a server to a client on discovery.
	//! The server assigns the client an ID which is unique for that server.
	struct ServiceDiscoveryResponse
	{
		uint64_t clientID;		//!< The unique client ID.
		uint16_t remotePort;	//!< The port the client should use for data.
	} AVS_PACKED;

	//! Features supported by a client.
	struct RenderingFeatures
	{
		bool normals=false;				//!< Whether normal maps are supported.
		bool ambientOcclusion=false;	//!< Whether ambient occlusion maps are supported.
	};
	//! The handshake sent by a connecting client to the server on initialization.
	//! Acknowledged by returning a avs::AcknowledgeHandshakeCommand to the client.
	struct Handshake
	{
		DisplayInfo startDisplayInfo = DisplayInfo();
		float MetresPerUnit = 1.0f;
		float FOV = 90.0f;
		uint32_t udpBufferSize = 0;			// In kilobytes.
		uint32_t maxBandwidthKpS = 0;		// In kilobytes per second
		AxesStandard axesStandard = AxesStandard::NotInitialized;
		uint8_t framerate = 0;				// In hertz
		bool usingHands = false; //Whether to send the hand nodes to the client.
		bool isVR = true;
		uint64_t resourceCount = 0;			//Count of resources the client has, and are appended to the handshake.
		uint32_t maxLightsSupported = 0;
		uint32_t clientStreamingPort = 0;	// the local port on the client to receive the stream.
		int32_t minimumPriority = 0;		// The lowest priority object this client will render, meshes with lower priority need not be sent.

		RenderingFeatures renderingFeatures;
	} AVS_PACKED;

	struct InputState
	{
		uint16_t numBinaryStates = 0;
		uint16_t numAnalogueStates= 0;
		uint16_t numBinaryEvents = 0;
		uint16_t numAnalogueEvents= 0;
		uint16_t numMotionEvents= 0;
	} AVS_PACKED;

	//Contains information to update the transform of a node.
	struct MovementUpdate
	{
		int64_t timestamp = 0;
		bool isGlobal = true;

		uid nodeID = 0;
		vec3 position;
		vec4 rotation;
		vec3 scale;

		vec3 velocity;
		vec3 angularVelocityAxis;
		float angularVelocityAngle = 0.0f;
	} AVS_PACKED;

	//TODO: Use instead of MovementUpdate for bandwidth.
	struct NodeUpdatePosition
	{
		int64_t timestamp = 0;
		bool isGlobal = true;

		uid nodeID = 0;
		vec3 position;
		vec3 velocity;
	} AVS_PACKED;

	//TODO: Use instead of MovementUpdate for bandwidth.
	struct NodeUpdateRotation
	{
		int64_t timestamp = 0;
		bool isGlobal = true;

		uid nodeID = 0;
		vec4 rotation;
		vec3 angularVelocityAxis;
		float angularVelocityAngle = 0.0f;
	} AVS_PACKED;

	// TODO: Use instead of MovementUpdate for bandwidth.
	struct NodeUpdateScale
	{
		int64_t timestamp = 0;
		bool isGlobal = true;

		uid nodeID = 0;
		vec3 scale;
		vec3 velocity;
	} AVS_PACKED;

	struct NodeUpdateEnabledState
	{
		uid nodeID = 0;			//< ID of the node we are changing the enabled state of.
		bool enabled = false;	//< Whether the node is enabled, and thus should be rendered.
	} AVS_PACKED;

	struct ApplyAnimation
	{
		int64_t timestamp = 0;	//< When the animation change was detected.
		uid nodeID = 0;			//< ID of the node the animation is playing on.
		uid animationID = 0;	//< ID of the animation that is now playing.
	} AVS_PACKED;

	// TODO: These enumerations are placeholder; in the future we want a more flexible system.
	//! Controls what is used as the current time of the animation.
	enum class AnimationTimeControl
	{
		ANIMATION_TIME=0,				//< Default; animation is controlled by time since animation started.
		CONTROLLER_0_TRIGGER,
		CONTROLLER_1_TRIGGER
	};

	struct NodeUpdateAnimationControl
	{
		uid nodeID = 0;						//< ID of the node the animation is playing on.
		uid animationID = 0;				//< ID of the animation that we are updating.

		AnimationTimeControl timeControl;	//< What controls the animation's time value.
	} AVS_PACKED;

	//! A message from a server to a client.
	//! The commandPayloadType specifies the size and interpretation of the packet.
	struct Command
	{
	//! What type of command this is, how to interpret it.
		CommandPayloadType commandPayloadType;

		Command(CommandPayloadType t) : commandPayloadType(t) {}

		//! Returns byte size of the command.
		virtual size_t getCommandSize() const = 0;
	} AVS_PACKED;
	
	//! The message sent by a server to a client on receipt of the client's handshake,
	//! confirming that the session can begin.
	//! size is variable, as visible node uid's are appended.
	struct AcknowledgeHandshakeCommand : public Command
	{
		AcknowledgeHandshakeCommand() : Command(CommandPayloadType::AcknowledgeHandshake) {}
		AcknowledgeHandshakeCommand(size_t visibleNodeCount) : Command(CommandPayloadType::AcknowledgeHandshake), visibleNodeCount(visibleNodeCount) {}
		
		virtual size_t getCommandSize() const override
		{
			return sizeof(AcknowledgeHandshakeCommand);
		}

		size_t visibleNodeCount = 0; //!<Count of visible node IDs appended to the command payload.
	} AVS_PACKED;

	//! Sent from server to client to set the origin of the client's space.
	struct SetPositionCommand : public Command
	{
		SetPositionCommand() : Command(CommandPayloadType::SetPosition) {}

		virtual size_t getCommandSize() const override
		{
			return sizeof(SetPositionCommand);
		}
		avs::uid origin_node=0;
		// TODO: replace these with a Pose.
		vec3 origin_pos;			//!< Position of the origin in world space.
		vec4 orientation;			//!< Orientation of the origin in world space.
		//!< A validity value. Larger values indicate newer data, so the client ignores messages with smaller validity than the last one received.
		uint64_t valid_counter = 0;
	} AVS_PACKED;

	struct SetupCommand : public Command
	{
		SetupCommand() : Command(CommandPayloadType::Setup) {}

		virtual size_t getCommandSize() const override
		{
			return sizeof(SetupCommand);
		}

		int32_t server_streaming_port = 0;
		int32_t server_http_port = 0;
		uint32_t debug_stream = 0;
		uint32_t do_checksums = 0;
		uint32_t debug_network_packets = 0;
		int32_t requiredLatencyMs = 0;
		uint32_t idle_connection_timeout = 5000;
		uid	server_id = 0;
		ControlModel control_model = ControlModel::NONE;
		VideoConfig video_config;
		float       draw_distance = 0.0f;
		vec3 bodyOffsetFromHead;
		AxesStandard axesStandard = AxesStandard::NotInitialized;
		uint8_t audio_input_enabled = 0;
		bool using_ssl = true;
		int64_t startTimestamp_utc_unix_ms = 0; //UTC Unix Timestamp in milliseconds of when the server started streaming to the client.
		BackgroundMode backgroundMode;
	} AVS_PACKED;

	//! Sends GI textures. The packet will be sizeof(SetupLightingCommand) + num_gi_textures uid's, each 64 bits.
	struct SetupLightingCommand : public Command
	{
		SetupLightingCommand() : Command(CommandPayloadType::SetupLighting) {}
		SetupLightingCommand(uint8_t numGI)
			:Command(CommandPayloadType::SetupLighting), num_gi_textures(numGI)
		{}

		virtual size_t getCommandSize() const override
		{
			return sizeof(SetupLightingCommand);
		}
		// If this is nonzero, implicitly gi should be enabled.
		uint8_t num_gi_textures=0;
	} AVS_PACKED;

	//! The definition of a single input that the server expects the client to provide when needed.
	struct InputDefinition
	{
		InputId inputId;
		InputType inputType;
		//! A regular expression that will be used to match the full component path of a client-side control
		std::string regexPath;
	} AVS_PACKED;

	struct InputDefinitionNetPacket
	{
		avs::InputId inputId;
		avs::InputType inputType;
		uint16_t pathLength;
	} AVS_PACKED;

	//! Sends GI textures. The packet will be sizeof(SetupLightingCommand) + num_gi_textures uid's, each 64 bits.
	struct SetupInputsCommand : public Command
	{
		SetupInputsCommand() : Command(CommandPayloadType::SetupInputs) {}
		SetupInputsCommand(uint8_t numi)
			:Command(CommandPayloadType::SetupInputs), numInputs(numi)
		{}

		virtual size_t getCommandSize() const override
		{
			return sizeof(SetupInputsCommand);
		}
		//! The number of inputs to follow the command.
		uint16_t numInputs = 0;
	} AVS_PACKED;

	struct ReconfigureVideoCommand : public Command
	{
		ReconfigureVideoCommand() : Command(CommandPayloadType::ReconfigureVideo) {}

		virtual size_t getCommandSize() const override
		{
			return sizeof(ReconfigureVideoCommand);
		}

		VideoConfig video_config;
	} AVS_PACKED;

	struct ShutdownCommand : public Command
	{
		ShutdownCommand() : Command(CommandPayloadType::Shutdown) {}

		virtual size_t getCommandSize() const override
		{
			return sizeof(ShutdownCommand);
		}
	} AVS_PACKED;

	struct NodeVisibilityCommand : public Command
	{
		size_t nodesShowCount;
		size_t nodesHideCount;

		NodeVisibilityCommand()
			:NodeVisibilityCommand(0, 0)
		{}

		NodeVisibilityCommand(size_t nodesShowCount, size_t nodesHideCount)
			:Command(CommandPayloadType::NodeVisibility), nodesShowCount(nodesShowCount), nodesHideCount(nodesHideCount)
		{}

		virtual size_t getCommandSize() const override
		{
			return sizeof(NodeVisibilityCommand);
		}
	} AVS_PACKED;

	struct UpdateNodeMovementCommand : public Command
	{
		size_t updatesCount;

		UpdateNodeMovementCommand()
			:UpdateNodeMovementCommand(0)
		{}

		UpdateNodeMovementCommand(size_t updatesCount)
			:Command(CommandPayloadType::UpdateNodeMovement), updatesCount(updatesCount)
		{}

		virtual size_t getCommandSize() const override
		{
			return sizeof(UpdateNodeMovementCommand);
		}
	} AVS_PACKED;
	
	struct UpdateNodeEnabledStateCommand : public Command
	{
		size_t updatesCount;

		UpdateNodeEnabledStateCommand()
			:UpdateNodeEnabledStateCommand(0)
		{}

		UpdateNodeEnabledStateCommand(size_t updatesCount)
			:Command(CommandPayloadType::UpdateNodeEnabledState), updatesCount(updatesCount)
		{}

		virtual size_t getCommandSize() const override
		{
			return sizeof(UpdateNodeEnabledStateCommand);
		}
	} AVS_PACKED;
	

	struct SetNodeHighlightedCommand : public Command
	{
		avs::uid nodeID = 0;
		bool isHighlighted = false;

		SetNodeHighlightedCommand()
			:SetNodeHighlightedCommand(0, false)
		{}

		SetNodeHighlightedCommand(avs::uid nodeID, bool isHighlighted)
			:Command(CommandPayloadType::SetNodeHighlighted), nodeID(nodeID), isHighlighted(isHighlighted)
		{}

		virtual size_t getCommandSize() const override
		{
			return sizeof(SetNodeHighlightedCommand);
		}
	} AVS_PACKED;

	struct UpdateNodeStructureCommand : public Command
	{
		uid nodeID = 0;
		uid parentID = 0;
		Pose relativePose;
		UpdateNodeStructureCommand()
			:UpdateNodeStructureCommand(0, 0,avs::Pose())
		{}

		UpdateNodeStructureCommand(avs::uid n, avs::uid p,avs::Pose relPose)
			:Command(CommandPayloadType::UpdateNodeStructure), nodeID(n), parentID(p)
			,relativePose(relPose)
		{}

		virtual size_t getCommandSize() const override
		{
			return sizeof(UpdateNodeStructureCommand);
		}
	} AVS_PACKED;

	//! A command to set the locally-tracked pose of a node, for example, a node can here be linked to a regex path for an OpenXR pose control.
	//! If pathLength>0, followed by a number of chars given in pathLength for the utf8 regex path. The nodeSubType is no longer used.
	struct UpdateNodeSubtypeCommand : public Command
	{
		uid nodeID = 0;
		NodeSubtype_deprecated nodeSubtype=NodeSubtype_deprecated::None;
		//! A regular expression that will be used to match the full component path of a client-side pose.
		uint16_t pathLength;
		UpdateNodeSubtypeCommand()
			:UpdateNodeSubtypeCommand(0, 0)
		{}

		UpdateNodeSubtypeCommand(avs::uid n,uint16_t pathl)
			:Command(CommandPayloadType::UpdateNodeSubtype), nodeID(n),nodeSubtype(NodeSubtype_deprecated::None), pathLength(pathl)
		{}

		virtual size_t getCommandSize() const override
		{
			return sizeof(UpdateNodeSubtypeCommand);
		}
	} AVS_PACKED;
	
	struct UpdateNodeAnimationCommand : public Command
	{
		avs::ApplyAnimation animationUpdate;

		UpdateNodeAnimationCommand()
			:UpdateNodeAnimationCommand(avs::ApplyAnimation{})
		{}

		UpdateNodeAnimationCommand(const avs::ApplyAnimation& update)
			:Command(CommandPayloadType::UpdateNodeAnimation), animationUpdate(update)
		{}

		virtual size_t getCommandSize() const override
		{
			return sizeof(UpdateNodeAnimationCommand);
		}
	} AVS_PACKED;

	struct SetAnimationControlCommand : public Command
	{
		avs::NodeUpdateAnimationControl animationControlUpdate;

		SetAnimationControlCommand()
			:SetAnimationControlCommand(avs::NodeUpdateAnimationControl{})
		{}

		SetAnimationControlCommand(const avs::NodeUpdateAnimationControl& update)
			:Command(CommandPayloadType::UpdateNodeAnimationControl), animationControlUpdate(update)
		{}

		virtual size_t getCommandSize() const override
		{
			return sizeof(SetAnimationControlCommand);
		}
	} AVS_PACKED;

	struct SetNodeAnimationSpeedCommand : public Command
	{
		uid nodeID = 0;
		uid animationID = 0;
		float speed = 1.0f;

		SetNodeAnimationSpeedCommand()
			:SetNodeAnimationSpeedCommand(0, 0, 1.0f)
		{}

		SetNodeAnimationSpeedCommand(uid nodeID, uid animationID, float speed)
			:Command(CommandPayloadType::SetNodeAnimationSpeed), nodeID(nodeID), animationID(animationID), speed(speed)
		{}

		virtual size_t getCommandSize() const override
		{
			return sizeof(SetNodeAnimationSpeedCommand);
		}
	} AVS_PACKED;

	/// A message from a client to a server.
	struct ClientMessage
	{
		/// Specifies what type of client message this is.
		ClientMessagePayloadType clientMessagePayloadType;

		ClientMessage(ClientMessagePayloadType t) : clientMessagePayloadType(t) {}

		/// Returns byte size of message.
		virtual size_t getMessageSize() const = 0;
	} AVS_PACKED;

	//! Message info struct containing how many nodes have changed to what state; sent alongside two lists of node UIDs.
	struct NodeStatusMessage : public ClientMessage
	{
	//! How many nodes the client is drawing. The node uid's will be appended first in the packet.
		size_t nodesDrawnCount;
	//! How many nodes the client has but is no longer drawing. The node uid's will be appended second in the packet.
		size_t nodesWantToReleaseCount;

		NodeStatusMessage()
			:NodeStatusMessage(0, 0)
		{}

		NodeStatusMessage(size_t nodesDrawnCount, size_t nodesWantToReleaseCount)
			:ClientMessage(ClientMessagePayloadType::NodeStatus),
			nodesDrawnCount(nodesDrawnCount),
			nodesWantToReleaseCount(nodesWantToReleaseCount)
		{}

		virtual size_t getMessageSize() const override
		{
			return sizeof(NodeStatusMessage);
		}
	} AVS_PACKED;

	//! Message info struct containing how many resources were received; sent alongside a list of UIDs.
	struct ReceivedResourcesMessage : public ClientMessage
	{
	//! How many resources were received. The uid's will be appended in the packet.
		size_t receivedResourcesCount;

		ReceivedResourcesMessage()
			:ReceivedResourcesMessage(0)
		{}

		ReceivedResourcesMessage(size_t receivedResourcesCount)
			:ClientMessage(ClientMessagePayloadType::ReceivedResources), receivedResourcesCount(receivedResourcesCount)
		{}

		virtual size_t getMessageSize() const override
		{
			return sizeof(ReceivedResourcesMessage);
		}
	} AVS_PACKED;

	//! Message info struct containing how many resources were received; sent alongside a list of UIDs.
	struct ControllerPosesMessage : public ClientMessage
	{
	//! The headset's pose.
		Pose headPose;
	//! Poses of the two controllers.
		Pose controllerPoses[2];

		ControllerPosesMessage()
			:ClientMessage(ClientMessagePayloadType::ControllerPoses)
		{}

		virtual size_t getMessageSize() const override
		{
			return sizeof(ControllerPosesMessage);
		}
	} AVS_PACKED;

	struct OriginPoseMessage : public ClientMessage
	{
		uint64_t counter = 0;
		avs::uid originId=0;
		Pose originPose;

		OriginPoseMessage() :ClientMessage(ClientMessagePayloadType::OriginPose) {}

		virtual size_t getMessageSize() const override
		{
			return sizeof(OriginPoseMessage);
		}
	} AVS_PACKED;
#ifdef _MSC_VER
#pragma pack(pop)
#endif
} //namespace avs
