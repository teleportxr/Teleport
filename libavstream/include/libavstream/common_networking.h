#pragma once

#include <cstdint>
#include <iostream>

#include "common.hpp"
#include "common_maths.h"

namespace avs
{
	#pragma pack(push)
	#pragma pack(1)
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

	enum class CommandPayloadType
	{
		Invalid,
		Shutdown,
		Setup,
		NodeBounds,
		AcknowledgeHandshake,
		SetPosition,
		UpdateNodeMovement,
		ReconfigureVideo
	};

	enum class ClientMessagePayloadType : uint8_t
	{
		Invalid,
		NodeStatus,
		ReceivedResources,
		ControllerPoses,
		OriginPose
	};

	struct Command
	{
		Command(CommandPayloadType t) : commandPayloadType(t) {}
		CommandPayloadType commandPayloadType;
	};

	struct AcknowledgeHandshakeCommand : public Command
	{
		AcknowledgeHandshakeCommand() : Command(CommandPayloadType::AcknowledgeHandshake) {}
		AcknowledgeHandshakeCommand(size_t visibleNodeAmount) : Command(CommandPayloadType::AcknowledgeHandshake), visibleNodeAmount(visibleNodeAmount) {}

		size_t visibleNodeAmount = 0; //Amount of visible node IDs appended to the command payload.
	};

	struct SetPositionCommand : public Command
	{
		SetPositionCommand() : Command(CommandPayloadType::SetPosition) {}

		vec3 origin_pos;
		vec4 orientation;
		uint64_t valid_counter = 0;
		uint8_t set_relative_pos = 0;
		vec3 relative_pos;
	};

	struct SetupCommand : public Command
	{
		SetupCommand() : Command(CommandPayloadType::Setup) {}

		int32_t server_streaming_port = 0;
		uint32_t debug_stream = 0;
		uint32_t do_checksums = 0;
		uint32_t debug_network_packets = 0;
		int32_t requiredLatencyMs = 0;
		uint32_t idle_connection_timeout = 5000;
		uid	server_id = 0;
		ControlModel control_model = ControlModel::NONE;
		VideoConfig video_config;
		vec3 bodyOffsetFromHead;
		AxesStandard axesStandard = AxesStandard::NotInitialized;
		uint8_t audio_input_enabled = 0;
	};

	struct ReconfigureVideoCommand : public Command
	{
		ReconfigureVideoCommand() : Command(CommandPayloadType::ReconfigureVideo) {}
		VideoConfig video_config;
	};

	struct ShutdownCommand : public Command
	{
		ShutdownCommand() : Command(CommandPayloadType::Shutdown) {}
	};

	struct NodeBoundsCommand : public Command
	{
		size_t nodesShowAmount;
		size_t nodesHideAmount;

		NodeBoundsCommand()
			:NodeBoundsCommand(0, 0)
		{}

		NodeBoundsCommand(size_t nodesShowAmount, size_t nodesHideAmount)
			:Command(CommandPayloadType::NodeBounds), nodesShowAmount(nodesShowAmount), nodesHideAmount(nodesHideAmount)
		{}
	};

	struct UpdateNodeMovementCommand : public Command
	{
		size_t updatesAmount;

		UpdateNodeMovementCommand()
			:UpdateNodeMovementCommand(0)
		{}

		UpdateNodeMovementCommand(size_t updatesAmount)
			:Command(CommandPayloadType::UpdateNodeMovement), updatesAmount(updatesAmount)
		{}
	};

	struct ClientMessage
	{
		ClientMessage(ClientMessagePayloadType t) : clientMessagePayloadType(t) {}
		ClientMessagePayloadType clientMessagePayloadType;
	};

	//Message info struct containing how many nodes have changed to what state; sent alongside two list of node UIDs.
	struct NodeStatusMessage : public ClientMessage
	{
		size_t nodesDrawnAmount;
		size_t nodesWantToReleaseAmount;

		NodeStatusMessage()
			:NodeStatusMessage(0, 0)
		{}

		NodeStatusMessage(size_t nodesDrawnAmount, size_t nodesWantToReleaseAmount)
			:ClientMessage(ClientMessagePayloadType::NodeStatus),
			nodesDrawnAmount(nodesDrawnAmount),
			nodesWantToReleaseAmount(nodesWantToReleaseAmount)
		{}
	};

	//Message info struct containing how many resources were received; sent alongside a list of UIDs.
	struct ReceivedResourcesMessage : public ClientMessage
	{
		size_t receivedResourcesAmount;

		ReceivedResourcesMessage()
			:ReceivedResourcesMessage(0)
		{}

		ReceivedResourcesMessage(size_t receivedResourcesAmount)
			:ClientMessage(ClientMessagePayloadType::ReceivedResources), receivedResourcesAmount(receivedResourcesAmount)
		{}
	};

	//Message info struct containing how many resources were received; sent alongside a list of UIDs.
	struct ControllerPosesMessage : public ClientMessage
	{
		Pose headPose;
		Pose controllerPoses[2];

		ControllerPosesMessage()
			:ClientMessage(ClientMessagePayloadType::ControllerPoses)
		{}
	};

	struct OriginPoseMessage : public ClientMessage
	{
		uint64_t counter = 0;
		Pose originPose;

		OriginPoseMessage() :ClientMessage(ClientMessagePayloadType::OriginPose) {}
	};

	struct Handshake
	{
		DisplayInfo startDisplayInfo = DisplayInfo();
		float MetresPerUnit = 1.0f;
		float FOV = 90.0f;
		uint32_t udpBufferSize = 0;			// In kilobytes.
		uint32_t maxBandwidthKpS = 0;			// In kilobytes per second
		AxesStandard axesStandard = AxesStandard::NotInitialized;
		uint8_t framerate = 0;				// In hertz
		bool usingHands = false; //Whether to send the hand nodes to the client.
		bool isVR = true;
		uint64_t resourceCount = 0; //Amount of resources the client has, and are appended to the handshake.
		uint32_t maxLightsSupported = 0;
		uint32_t clientStreamingPort = 0; // the local port on the client to receive the stream.
	};

	struct InputState
	{
		uint32_t controllerId = 0;
		uint32_t buttonsDown = 0;		// arbitrary bitfield.
		float trackpadAxisX = 0.0f;
		float trackpadAxisY = 0.0f;
		float joystickAxisX = 0.0f;
		float joystickAxisY = 0.0f;

		uint32_t binaryEventAmount = 0;
		uint32_t analogueEventAmount = 0;
		uint32_t motionEventAmount = 0;
	};

	//Contains information to update the transform of a node.
	struct MovementUpdate
	{
		std::uint64_t timestamp = 0;
		bool isGlobal = true;

		uid nodeID = 0;
		vec3 position;
		vec4 rotation;

		vec3 velocity;
		vec3 angularVelocityAxis;
		float angularVelocityAngle = 0.0f;
	};

	inline size_t GetCommandSize(CommandPayloadType t)
	{
		switch(t)
		{
		case CommandPayloadType::Setup:
			return sizeof(SetupCommand);
		case CommandPayloadType::Shutdown:
			return sizeof(ShutdownCommand);
		case CommandPayloadType::NodeBounds:
			return sizeof(NodeBoundsCommand);
		case CommandPayloadType::AcknowledgeHandshake:
			return sizeof(AcknowledgeHandshakeCommand);
		case CommandPayloadType::SetPosition:
			return sizeof(SetPositionCommand);
		case CommandPayloadType::UpdateNodeMovement:
			return sizeof(UpdateNodeMovementCommand);
		default:
			return 0;
		};
	}

	inline size_t GetClientMessageSize(ClientMessagePayloadType t)
	{
		switch(t)
		{
		case ClientMessagePayloadType::NodeStatus:
			return sizeof(NodeStatusMessage);
		case ClientMessagePayloadType::ReceivedResources:
			return sizeof(ReceivedResourcesMessage);
		case ClientMessagePayloadType::ControllerPoses:
			return sizeof(ControllerPosesMessage);
		case ClientMessagePayloadType::OriginPose:
			return sizeof(OriginPoseMessage);
		default:
			std::cerr << "Unrecognised ClientMessagePayloadType in GetClientMessageSize\n";
			return 0;
		};
	}
#pragma pack(pop)
} //namespace avs
