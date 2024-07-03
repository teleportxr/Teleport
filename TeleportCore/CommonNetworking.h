#pragma once

#include <cstdint>
#include <iostream>

#include "libavstream/common.hpp"
#include "libavstream/common_maths.h"
#include "libavstream/common_input.h"

#ifdef _MSC_VER
#pragma pack(push, 1)
typedef vec2 vec2_packed;
typedef vec3 vec3_packed;
typedef vec4 vec4_packed;
#else
#pragma pack(1)
struct vec2_packed
{
	float x, y;
	operator const vec2() const {
		return *((const vec2*)this);
	}
} AVS_PACKED;
struct vec3_packed
{
	float x, y, z;
	operator const vec3() const {
		return *((const vec3*)this);
	}
} AVS_PACKED;
struct vec4_packed
{
	float x, y, z, w;
	operator const vec4() const {
		return *((const vec4*)this);
	}
} AVS_PACKED;
#endif
namespace teleport
{
	namespace core
	{
		enum class BackgroundMode : uint8_t
		{
			NONE = 0, COLOUR, TEXTURE, VIDEO
		} AVS_PACKED;
#ifdef _MSC_VER
		typedef avs::Pose Pose_packed;
		typedef avs::PoseDynamic PoseDynamic_packed;
#else
		struct Pose_packed
		{
			void operator=(const avs::Pose &p)
			{
				orientation=*((const vec4_packed*)&p.orientation);
				position=*((const vec3_packed*)&p.position);
			}
			vec4_packed orientation = { 0, 0, 0, 1 };
			vec3_packed position = { 0, 0, 0 };
		} AVS_PACKED;
		struct PoseDynamic_packed
		{
			void operator=(const avs::PoseDynamic &p)
			{
				pose=p.pose;
				velocity=*((const vec3_packed*)&p.velocity);
				angularVelocity=*((const vec3_packed*)&p.angularVelocity);
			}
			Pose_packed pose;
			vec3_packed velocity;
			vec3_packed angularVelocity;
		} AVS_PACKED;
#endif
		enum class RemotePlaySessionChannel : unsigned char 
		{
			RPCH_Handshake = 0,
			RPCH_Control = 1,
			RPCH_StreamingControl= 7,
			RPCH_NumChannels
		} AVS_PACKED;

		enum class ControlModel : uint32_t
		{
			NONE=0,
			SERVER_ORIGIN_CLIENT_LOCAL=2
		} AVS_PACKED;

		enum class NodeStatus : uint8_t
		{
			Unknown = 0,
			Drawn,
			WantToRelease,
			Released
		} AVS_PACKED;
	
		//! The payload type, or how to interpret the server's message.
		enum class CommandPayloadType : uint8_t
		{									
			Invalid=0,	
			Shutdown,
			Setup,
			AcknowledgeHandshake,
			ReconfigureVideo,
			NodeVisibility,					// id
			UpdateNodeMovement,				//
			UpdateNodeEnabledState,			// id
			SetNodeHighlighted,				// id
			ApplyNodeAnimation,				// id
			UpdateNodeAnimationControlX,
			SetNodeAnimationSpeed,			// id
			SetupLighting,					// 0
			UpdateNodeStructure,			// id
			AssignNodePosePath,				// id
			SetupInputs,					// 0
			PingForLatency,
			SetStageSpaceOriginNode=128		// 0
		} AVS_PACKED;
		inline const char *StringOf(CommandPayloadType type)
		{
		switch(type)
		{
			case CommandPayloadType::Invalid				   :return "Invalid";
			case CommandPayloadType::Shutdown				   :return "Shutdown";
			case CommandPayloadType::Setup					   :return "Setup";
			case CommandPayloadType::AcknowledgeHandshake	   :return "AcknowledgeHandshake";
			case CommandPayloadType::ReconfigureVideo		   :return "ReconfigureVideo";
			case CommandPayloadType::SetStageSpaceOriginNode   :return "SetStageSpaceOriginNode";
			case CommandPayloadType::NodeVisibility			   :return "NodeVisibility";
			case CommandPayloadType::UpdateNodeMovement		   :return "UpdateNodeMovement";
			case CommandPayloadType::UpdateNodeEnabledState	   :return "UpdateNodeEnabledState";
			case CommandPayloadType::SetNodeHighlighted		   :return "SetNodeHighlighted";
			case CommandPayloadType::ApplyNodeAnimation			:return "ApplyNodeAnimation";
			case CommandPayloadType::UpdateNodeAnimationControlX:return "UpdateNodeAnimationControlX";
			case CommandPayloadType::SetNodeAnimationSpeed	   :return "SetNodeAnimationSpeed";
			case CommandPayloadType::SetupLighting			   :return "SetupLighting";
			case CommandPayloadType::UpdateNodeStructure	   :return "UpdateNodeStructure";
			case CommandPayloadType::AssignNodePosePath		   :return "AssignNodePosePath";
			case CommandPayloadType::SetupInputs			   :return "SetupInputs";
		default:
		return "Invalid";
		};
		}
		//! The payload type, or how to interpret the client's message.
		enum class ClientMessagePayloadType : uint8_t
		{
			Invalid=0,
			NodeStatus,
			ReceivedResources,
			ControllerPoses,
			ResourceLost,		//! Inform the server that client "lost" a previously confirmed resource, e.g. due to some bug or error. Should *rarely* be used.
			InputStates,
			InputEvents,
			DisplayInfo,
			KeyframeRequest,
			PongForLatency,
			OrthogonalAcknowledgement,
			Acknowledgement
		} AVS_PACKED;
	
		//! The response payload sent by a server to a client on discovery.
		//! The server assigns the client an ID which is unique for that server.
		struct ServiceDiscoveryResponse
		{
			uint64_t clientID;		//!< The unique client ID.
			uint16_t remotePort;	//!< The port the client should use for data.
		} AVS_PACKED;

		//! The handshake sent by a connecting client to the server on initialization.
		//! Acknowledged by returning a avs::AcknowledgeHandshakeCommand to the client.
		struct Handshake
		{
			avs::DisplayInfo startDisplayInfo = avs::DisplayInfo();
			float MetresPerUnit = 1.0f;
			float FOV = 90.0f;
			uint32_t udpBufferSize = 0;			// In kilobytes.
			uint32_t maxBandwidthKpS = 0;		// In kilobytes per second
			avs::AxesStandard axesStandard = avs::AxesStandard::NotInitialized;
			uint8_t framerate = 0;				// In hertz
			bool isVR = true;
			uint64_t resourceCount = 0;			//Count of resources the client has, and are appended to the handshake.
			uint32_t maxLightsSupported = 0;
			int32_t minimumPriority = 0;		// The lowest priority object this client will render, meshes with lower priority need not be sent.
			avs::RenderingFeatures renderingFeatures;
		} AVS_PACKED;

		struct InputState
		{
			uint16_t numBinaryStates = 0;
			uint16_t numAnalogueStates= 0;
		} AVS_PACKED;

		//Contains information to update the transform of a node.
		struct MovementUpdate
		{
			int64_t server_time_us = 0;		// microseconds since the SetupCommand's timestamp.
			bool isGlobal = true;

			avs::uid nodeID = 0;
			vec3_packed position={0,0,0};
			vec4_packed rotation={0,0,0,0};
			vec3_packed scale={0,0,0};

			vec3_packed velocity={0,0,0};
			vec3_packed angularVelocityAxis={0,0,0};
			float angularVelocityAngle = 0.0f;
		} AVS_PACKED;

		//TODO: Use instead of MovementUpdate for bandwidth.
		struct NodeUpdatePosition
		{
			int64_t timestamp = 0;
			bool isGlobal = true;

			avs::uid nodeID = 0;
			vec3_packed position;
			vec3_packed velocity;
		} AVS_PACKED;

		//TODO: Use instead of MovementUpdate for bandwidth.
		struct NodeUpdateRotation
		{
			int64_t timestamp = 0;
			bool isGlobal = true;

			avs::uid nodeID = 0;
			vec4_packed rotation;
			vec3_packed angularVelocityAxis;
			float angularVelocityAngle = 0.0f;
		} AVS_PACKED;

		// TODO: Use instead of MovementUpdate for bandwidth.
		struct NodeUpdateScale
		{
			int64_t timestamp = 0;
			bool isGlobal = true;

			avs::uid nodeID = 0;
			vec3_packed scale;
			vec3_packed scale_rate;
		} AVS_PACKED;

		struct NodeUpdateEnabledState
		{
			avs::uid nodeID = 0;	//< ID of the node we are changing the enabled state of.
			bool enabled = false;	//< Whether the node is enabled, and thus should be rendered.
		} AVS_PACKED;

		//! Animation is applied to skeletons (hierarchies of nodes).
		//! Each skeleton can have one or more animation layers, and each layer is applied in turn.
		//! Each layer has either zero, one, or two animations active.
		//! This is because it can be in three states: not playing an animation, playing one animation,
		//!  or transitioning from the previous animation to the next.
		//! In ApplyAnimation, we specify a time at which the named animation is fully applied.
		//! If the client is playing a different animation on this skeleton node and layer, and the timestamp
		//! has not yet been reached, it will interpolate to the new state.
		struct ApplyAnimation
		{
			int32_t animLayer=0;			
			int64_t timestampUs = 0;					//< When the animation state should apply, in server-session time, microseconds.
			avs::uid nodeID = 0;						//< ID of the node the animation is playing on.
			avs::uid animationID = 0;					//< ID of the animation that is now playing.
			float animTimeAtTimestamp=0.0f;				//< At the given timestamp, where in the animation should we be?
			float speedUnitsPerSecond=1.0f;				//< At the timestamp, how fast is the animation playing? For extrapolation.
			bool loop=false;
		} AVS_PACKED;

		//! A message from a server to a client.
		//! The commandPayloadType specifies the size and interpretation of the packet.
		struct Command
		{
		//! What type of command this is, how to interpret it.
			CommandPayloadType commandPayloadType;
			Command(CommandPayloadType t) : commandPayloadType(t) {}

		} AVS_PACKED;
	
		//! The message sent by a server to a client on receipt of the client's handshake,
		//! confirming that the session can begin.
		//! size is variable, as visible node uid's are appended.
		struct AcknowledgeHandshakeCommand : public Command
		{
			AcknowledgeHandshakeCommand() : Command(CommandPayloadType::AcknowledgeHandshake) {}
			AcknowledgeHandshakeCommand(size_t visibleNodeCount) : Command(CommandPayloadType::AcknowledgeHandshake), visibleNodeCount(visibleNodeCount) {}
		
			static size_t getCommandSize()
			{
				return sizeof(AcknowledgeHandshakeCommand);
			}

			size_t visibleNodeCount = 0; //!<Count of visible node IDs appended to the command payload.
		} AVS_PACKED;

		//! Mode specifying how to light objects.
		enum class LightingMode : uint8_t
		{
			NONE = 0,
			TEXTURE,
			VIDEO
		} AVS_PACKED;
		//! Setup for dynamically-lit objects on the client.
		struct ClientDynamicLighting
		{
			int2 specularPos = {0, 0};
			int32_t specularCubemapSize = 0;
			int32_t specularMips = 0;
			int2 diffusePos = {0, 0};
			int32_t diffuseCubemapSize = 0;
			int2 lightPos = {0, 0};
			int32_t lightCubemapSize = 0;
			avs::uid specular_cubemap_texture_uid = 0;
			avs::uid diffuse_cubemap_texture_uid = 0;	//14*4=56
			LightingMode lightingMode = LightingMode::TEXTURE;// 57
		} AVS_PACKED; // 57 bytes
		static_assert (sizeof(ClientDynamicLighting) == 57, "ClientDynamicLighting Size is not correct");
		//! The setup information sent by the server on connection to a given client.
		struct SetupCommand : public Command
		{
			SetupCommand() : Command(CommandPayloadType::Setup) {}

			static size_t getCommandSize()
			{
				return sizeof(SetupCommand);
			}																		//   Command=1
			uint32_t			debug_stream = 0;									//!< 1+4=5
			uint32_t			debug_network_packets = 0;							//!< 5+4=9
			int32_t				requiredLatencyMs = 0;								//!< 9+4=13
			uint32_t			idle_connection_timeout = 5000;						//!< 13+4=17
			uint64_t			session_id = 0;										//!< 17+8=25	The server's session id changes when the server session changes.	37 bytes
			avs::VideoConfig	video_config;										//!< 25+89=114	Video setup structure. 41+89=130 bytes
			float				draw_distance = 0.0f;								//!< 114+4=118	Maximum distance in metres to render locally. 134
			avs::AxesStandard	axesStandard = avs::AxesStandard::NotInitialized;	//!< 118+1=119	The axis standard that the server uses, may be different from the client's. 147
			uint8_t				audio_input_enabled = 0;							//!< 119+1=120	Server accepts audio stream from client.
			bool				using_ssl = true;									//!< 120+1=121	Not in use, for later.
			int64_t				startTimestamp_utc_unix_us = 0;						//!< 121+8=129	UTC Unix Timestamp in microseconds when the server session began.
			// TODO: replace this with a background Material, which MAY contain video, te			xture and/or plain colours.
			BackgroundMode		backgroundMode;										//!< 129+1=130	Whether the server supplies a background, and of which type.
			vec4_packed			backgroundColour;									//!< 130+16=146 If the background is of the COLOUR type, which colour to use.
			ClientDynamicLighting clientDynamicLighting;							//!<			Setup for dynamic object lighting. 174+57=231 bytes
			avs::uid			backgroundTexture=0;
		} AVS_PACKED;
		static_assert (sizeof(SetupCommand) == 211, "SetupCommand Size is not correct");

		//! Sends GI textures. The packet will be sizeof(SetupLightingCommand) + num_gi_textures uid's, each 64 bits.
		struct SetupLightingCommand : public Command
		{
			SetupLightingCommand() : Command(CommandPayloadType::SetupLighting) {}
			SetupLightingCommand(uint8_t numGI)
				:Command(CommandPayloadType::SetupLighting), num_gi_textures(numGI)
			{}

			static size_t getCommandSize()
			{
				return sizeof(SetupLightingCommand);
			}
			//! If this is nonzero, implicitly gi should be enabled.
			uint8_t num_gi_textures=0;
		} AVS_PACKED;

		//! A command that expects an acknowledgement of receipt from the client using an AcknowledgementMessage.
		struct AckedCommand : public Command
		{
			AckedCommand(CommandPayloadType t) : Command(t) {}
			//! The id that is used to acknowledge receipt via AcknowledgementMessage. Should increase monotonically per-full-client-session: clients can ignore any id less than or equal to a previously received id.
			uint64_t		ack_id = 0;
		} AVS_PACKED;
		
		//! Sent from server to client to set the origin of the client's space.
		struct SetStageSpaceOriginNodeCommand : public AckedCommand
		{
			SetStageSpaceOriginNodeCommand() : AckedCommand(CommandPayloadType::SetStageSpaceOriginNode) {}

			static size_t getCommandSize()
			{
				return sizeof(SetStageSpaceOriginNodeCommand);
			}
			avs::uid		origin_node=0;		//!< The session uid of the node to use as the origin.
			//! A validity value. Larger values indicate newer data, so the client ignores messages with smaller validity than the last one received.
			uint64_t		valid_counter = 0;
		} AVS_PACKED;

		//! The definition of a single input that the server expects the client to provide when needed.
		struct InputDefinition
		{
			avs::InputId inputId;
			avs::InputType inputType;
			//! A regular expression that will be used to match the full component path of a client-side control
			std::string regexPath;
		} AVS_PACKED;

		//! The netpacket version of InputDefinition, to be followed by pathLength utf8 characters.
		struct InputDefinitionNetPacket
		{
			avs::InputId inputId;
			avs::InputType inputType;
			uint16_t pathLength;
		} AVS_PACKED;

		//! Sends Input definitions. The packet will be sizeof(SetupLightingCommand) + num_gi_textures uid's, each 64 bits.
		struct SetupInputsCommand : public Command
		{
			SetupInputsCommand() : Command(CommandPayloadType::SetupInputs) {}
			SetupInputsCommand(uint8_t numi)
				:Command(CommandPayloadType::SetupInputs), numInputs(numi)
			{}

			static size_t getCommandSize()
			{
				return sizeof(SetupInputsCommand);
			}
			//! The number of inputs to follow the command.
			uint16_t numInputs = 0;
		} AVS_PACKED;
		//! Instructs the client to accept a new video configuration, e.g. if bandwidth requires a change of resolution.
		struct ReconfigureVideoCommand : public Command
		{
			ReconfigureVideoCommand() : Command(CommandPayloadType::ReconfigureVideo) {}

			static size_t getCommandSize()
			{
				return sizeof(ReconfigureVideoCommand);
			}
			//! The configuration to use.
			avs::VideoConfig video_config;
		} AVS_PACKED;

		//! Instructs the client to close the connection.
		struct ShutdownCommand : public Command
		{
			ShutdownCommand() : Command(CommandPayloadType::Shutdown) {}

			static size_t getCommandSize()
			{
				return sizeof(ShutdownCommand);
			}
		} AVS_PACKED;

		//! Instructs the client to show or hide the specified nodes.
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

			static size_t getCommandSize()
			{
				return sizeof(NodeVisibilityCommand);
			}
		} AVS_PACKED;

		//! Instructs the client to modify the motion of the specified nodes.
		struct UpdateNodeMovementCommand : public Command
		{
			size_t updatesCount;	//!< How many updates are included.

			UpdateNodeMovementCommand()
				:UpdateNodeMovementCommand(0)
			{}

			UpdateNodeMovementCommand(size_t updatesCount)
				:Command(CommandPayloadType::UpdateNodeMovement), updatesCount(updatesCount)
			{}

			static size_t getCommandSize()
			{
				return sizeof(UpdateNodeMovementCommand);
			}
		} AVS_PACKED;

		//! Instructs the client to modify the enabled state of the specified nodes.
		struct UpdateNodeEnabledStateCommand : public Command
		{
			size_t updatesCount;	//!< How many updates are included.

			UpdateNodeEnabledStateCommand()
				:UpdateNodeEnabledStateCommand(0)
			{}

			UpdateNodeEnabledStateCommand(size_t updatesCount)
				:Command(CommandPayloadType::UpdateNodeEnabledState), updatesCount(updatesCount)
			{}

			static size_t getCommandSize()
			{
				return sizeof(UpdateNodeEnabledStateCommand);
			}
		} AVS_PACKED;

		//! Instructs the client to modify the highlighted state of the specified nodes.
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

			static size_t getCommandSize()
			{
				return sizeof(SetNodeHighlightedCommand);
			}
		} AVS_PACKED;
		//! A confirmable state.
		struct NodeStateCommand : public Command
		{
			avs::uid nodeID = 0;		//!< Which node to modify.
			uint64_t confirmationNumber=0;
			NodeStateCommand(CommandPayloadType type,avs::uid nid)
				:Command(type),nodeID(nid)
			{}
		} AVS_PACKED;
		//! Instructs the client to reparent the specified node.
		struct UpdateNodeStructureCommand : public NodeStateCommand
		{
			avs::uid parentID = 0;		//!< The new parent uid.
			Pose_packed relativePose;	//!< The new relative pose of the child node.
			UpdateNodeStructureCommand()
				:UpdateNodeStructureCommand(0, 0,avs::Pose())
			{}

			UpdateNodeStructureCommand(avs::uid n, avs::uid p,avs::Pose relPose)
				:NodeStateCommand(CommandPayloadType::UpdateNodeStructure,n), parentID(p)
			{
				relativePose.position = *((vec3_packed *)&relPose.position);
				relativePose.orientation = *((vec4_packed *)&relPose.orientation);
			}

			static size_t getCommandSize()
			{
				return sizeof(UpdateNodeStructureCommand);
			}
		} AVS_PACKED;

		//! A command to set the locally-tracked pose of a node, for example, a node can here be linked to a regex path for an OpenXR pose control.
		//! If pathLength>0, followed by a number of chars given in pathLength for the utf8 regex path.
		//! If pathLength==0, control of the node is returned to the server.
		struct AssignNodePosePathCommand : public Command
		{
			avs::uid nodeID = 0;
			//! A regular expression that will be used to match the full component path of a client-side pose.
			uint16_t pathLength;
			AssignNodePosePathCommand()
				:AssignNodePosePathCommand(0, 0)
			{}

			AssignNodePosePathCommand(avs::uid n,uint16_t pathl)
				:Command(CommandPayloadType::AssignNodePosePath), nodeID(n), pathLength(pathl)
			{}

			static size_t getCommandSize()
			{
				return sizeof(AssignNodePosePathCommand);
			}
		} AVS_PACKED;
	
		//! Update the animation state of the specified nodes.
		struct ApplyAnimationCommand : public Command
		{
			ApplyAnimation animationUpdate;

			ApplyAnimationCommand() 
				:ApplyAnimationCommand(ApplyAnimation{})
			{}

			ApplyAnimationCommand(const ApplyAnimation& update)
				:Command(CommandPayloadType::ApplyNodeAnimation), animationUpdate(update)
			{}

			static size_t getCommandSize()
			{
				return sizeof(ApplyAnimationCommand);
			}
		} AVS_PACKED;

		struct SetNodeAnimationSpeedCommand : public Command
		{
			avs::uid nodeID = 0;
			avs::uid animationID = 0;
			float speed = 1.0f;

			SetNodeAnimationSpeedCommand()
				:SetNodeAnimationSpeedCommand(0, 0, 1.0f)
			{}

			SetNodeAnimationSpeedCommand(avs::uid nodeID, avs::uid animationID, float speed)
				:Command(CommandPayloadType::SetNodeAnimationSpeed), nodeID(nodeID), animationID(animationID), speed(speed)
			{}

			static size_t getCommandSize()
			{
				return sizeof(SetNodeAnimationSpeedCommand);
			}
		} AVS_PACKED;

		struct PingForLatencyCommand : public Command
		{
			int64_t unix_time_ns=0;

			PingForLatencyCommand()
				:Command(CommandPayloadType::PingForLatency)
			{}

			static size_t getCommandSize()
			{
				return sizeof(PingForLatencyCommand);
			}
		} AVS_PACKED; 

		/// A message from a client to a server.
		struct ClientMessage
		{
			/// Specifies what type of client message this is.
			ClientMessagePayloadType clientMessagePayloadType;
			uint64_t timestamp_unix_ms = 0;
			ClientMessage(ClientMessagePayloadType t) : clientMessagePayloadType(t) {}

		} AVS_PACKED;
		// TODO: this should be a separate message type, not a client message.
		struct OrthogonalAcknowledgementMessage: public ClientMessage
		{
			uint64_t confirmationNumber=0;
			OrthogonalAcknowledgementMessage()
				:OrthogonalAcknowledgementMessage(0)
			{}
			OrthogonalAcknowledgementMessage(uint64_t confirmationNumber)
				:ClientMessage(ClientMessagePayloadType::OrthogonalAcknowledgement),
				confirmationNumber(confirmationNumber)
			{}
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
		} AVS_PACKED;
		struct NodePose
		{
			avs::uid uid;
			PoseDynamic_packed poseDynamic;// in stage space.
		} AVS_PACKED;

		//! Message info struct containing head and other node poses. followed by numPoses NodePose structs.
		struct ControllerPosesMessage : public ClientMessage
		{
		//! The headset's pose.
			Pose_packed headPose;
		//! Poses of the  controllers.
			uint16_t numPoses=0;

			ControllerPosesMessage()
				:ClientMessage(ClientMessagePayloadType::ControllerPoses)
			{}
		} AVS_PACKED;
		
		//! Acknowledges receipt of an AckedCommand.
		struct AcknowledgementMessage : public ClientMessage
		{
			uint64_t ack_id=0;
			AcknowledgementMessage()
				:ClientMessage(ClientMessagePayloadType::Acknowledgement)
			{}
		} AVS_PACKED;

		//! Message info struct containing a request for resources, to be followed by the list of uid's.
		struct ResourceLostMessage : public ClientMessage
		{
			//! Number of resource uid's to follow the structure body.
			uint16_t resourceCount = 0;

			ResourceLostMessage()
				:ClientMessage(ClientMessagePayloadType::ResourceLost)
			{}
		} AVS_PACKED;

		//! Message info struct containing a request for resources, to be followed by the list of uid's.
		struct InputStatesMessage : public ClientMessage
		{
			//! Poses of the controllers.
			InputState inputState = {};
			InputStatesMessage()
				:ClientMessage(ClientMessagePayloadType::InputStates)
			{}
		} AVS_PACKED;

		struct InputEventsMessage : public ClientMessage
		{
			uint16_t numBinaryEvents = 0;
			uint16_t numAnalogueEvents = 0;
			uint16_t numMotionEvents = 0;
			InputEventsMessage()
				:ClientMessage(ClientMessagePayloadType::InputEvents)
			{}
		} AVS_PACKED;

		struct DisplayInfoMessage : public ClientMessage
		{
			avs::DisplayInfo displayInfo;
			DisplayInfoMessage()
				:ClientMessage(ClientMessagePayloadType::DisplayInfo)
			{}
		} AVS_PACKED;

		struct KeyframeRequestMessage:public ClientMessage
		{
			KeyframeRequestMessage()
				:ClientMessage(ClientMessagePayloadType::KeyframeRequest)
			{}
		} AVS_PACKED;

		struct PongForLatencyMessage :public ClientMessage
		{
			int64_t unix_time_ns = 0;
			int64_t server_to_client_latency_ns = 0;
			PongForLatencyMessage()
				:ClientMessage(ClientMessagePayloadType::PongForLatency)
			{}
		} AVS_PACKED;

		enum class SignalingState : uint8_t
		{
			START,			// Received a WebSocket connection.
			REQUESTED,		// Got an initial connection request message
			ACCEPTED,		// Accepted the connection request. Create a ClientData for this client if not already extant.
			STREAMING,		// Completed initial signaling, now using the signaling socket for streaming setup.
			INVALID
		};
		struct ClientNetworkState
		{
			float server_to_client_latency_ms = 0.0f;
			float client_to_server_latency_ms = 0.0f;
			core::SignalingState signalingState = core::SignalingState::INVALID;
			avs::StreamingConnectionState streamingConnectionState = avs::StreamingConnectionState::ERROR_STATE;
		};
	} //namespace 

}
#ifdef _MSC_VER
#pragma pack(pop)
#else
#pragma pack()
#endif