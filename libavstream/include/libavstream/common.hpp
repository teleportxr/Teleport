// libavstream
// (c) Copyright 2018-2019 Simul Software Ltd

#pragma once

#include <cassert>
#include <cstdint>
#include <iostream>
#include <libavstream/abi.hpp>
#include <vector>
#include <cmath>

#define LIBAVSTREAM_VERSION 1

namespace avs
{
	struct vec2
	{
		float x, y;

		vec2()
			:vec2(0.0f, 0.0f)
		{}

		vec2(float x, float y)
			:x(x), y(y)
		{}

		vec2 operator-() const
		{
			return vec2(-x, -y);
		}

		vec2 operator+(const vec2& rhs) const
		{
			return vec2(x + rhs.x, y + rhs.y);
		}

		vec2 operator-(const vec2& rhs) const
		{
			return vec2(x - rhs.x, y - rhs.y);
		}

		vec2 operator*(float rhs) const
		{
			return vec2(x * rhs, y * rhs);
		}

		vec2 operator/(float rhs) const
		{
			return vec2(x / rhs, y / rhs);
		}

		void operator+=(const vec2& rhs)
		{
			*this = *this + rhs;
		}

		void operator-=(const vec2& rhs)
		{
			*this = *this - rhs;
		}

		void operator*=(const float& rhs)
		{
			*this = *this * rhs;
		}

		void operator/=(const float& rhs)
		{
			*this = *this / rhs;
		}

		float Length() const
		{
			return std::sqrtf(x * x + y * y);
		}

		vec2 Normalised() const
		{
			return *this / Length();
		}

		float Dot(const vec2& other) const
		{
			return x * other.x + y * other.y;
		}

		template<typename OutStream>
		friend OutStream& operator<< (OutStream& out, const vec2& vec)
		{
			return out << vec.x << " " << vec.y;
		}

		template<typename InStream>
		friend InStream& operator>> (InStream& in, vec2& vec)
		{
			return in >> vec.x >> vec.y;
		}
	};

	struct vec3
	{
		float x, y, z;

		vec3()
			:vec3(0.0f, 0.0f, 0.0f)
		{}

		vec3(float x, float y, float z)
			:x(x), y(y), z(z)
		{}

		vec3 operator-() const
		{
			return vec3(-x, -y, -z);
		}

		vec3 operator+(const vec3& rhs) const
		{
			return vec3(x + rhs.x, y + rhs.y, z + rhs.z);
		}

		vec3 operator-(const vec3& rhs) const
		{
			return vec3(x - rhs.x, y - rhs.y, z - rhs.z);
		}

		vec3 operator*(float rhs) const
		{
			return vec3(x * rhs, y * rhs, z * rhs);
		}

		vec3 operator/(float rhs) const
		{
			return vec3(x / rhs, y / rhs, z / rhs);
		}

		void operator+=(const vec3& rhs)
		{
			*this = *this + rhs;
		}

		void operator-=(const vec3& rhs)
		{
			*this = *this - rhs;
		}

		void operator*=(float rhs)
		{
			*this = *this * rhs;
		}

		void operator/=(float rhs)
		{
			*this = *this / rhs;
		}

		float Length() const
		{
			return sqrtf(x * x + y * y + z * z);
		}

		vec3 Normalised() const
		{
			return *this / Length();
		}

		float Dot(const vec3& rhs) const
		{
			return x * rhs.x + y * rhs.y + z * rhs.z;
		}

		vec3 Cross(const vec3& rhs) const
		{
			return vec3(y * rhs.z - z * rhs.y, z * rhs.x - x * rhs.z, x * rhs.y - y * rhs.x);
		}

		template<typename OutStream>
		friend OutStream& operator<< (OutStream& out, const vec3& vec)
		{
			return out << vec.x << " " << vec.y << " " << vec.z;
		}

		template<typename InStream>
		friend InStream& operator>> (InStream& in, vec3& vec)
		{
			return in >> vec.x >> vec.y >> vec.z;
		}
	};

	struct vec4
	{
		float x, y, z, w;

		vec4()
			:vec4(0.0f, 0.0f, 0.0f, 0.0f)
		{}

		vec4(float x, float y, float z, float w)
			:x(x), y(y), z(z), w(w)
		{}

		vec4 operator-() const
		{
			return vec4(-x, -y, -z, -w);
		}

		vec4 operator+(const vec4& rhs) const
		{
			return vec4(x + rhs.x, y + rhs.y, z + rhs.z, w + rhs.w);
		}

		vec4 operator-(const vec4& rhs) const
		{
			return vec4(x - rhs.x, y - rhs.y, z - rhs.z, w - rhs.w);
		}

		vec4 operator*(float rhs) const
		{
			return vec4(x * rhs, y * rhs, z * rhs, w * rhs);
		}

		vec4 operator/(float rhs) const
		{
			return vec4(x / rhs, y / rhs, z / rhs, w / rhs);
		}

		void operator+=(const vec4& rhs)
		{
			*this = *this + rhs;
		}

		void operator-=(const vec4& rhs)
		{
			*this = *this - rhs;
		}

		void operator*=(float rhs)
		{
			*this = *this * rhs;
		}

		void operator/=(float rhs)
		{
			*this = *this / rhs;
		}

		float Length() const
		{
			return sqrtf(x * x + y * y + z * z + w * w);
		}

		vec4 Normalised() const
		{
			return *this / Length();
		}

		float Dot(const vec4& rhs) const
		{
			return x * rhs.x + y * rhs.y + z * rhs.z + w * rhs.w;
		}

		template<typename OutStream>
		friend OutStream& operator<< (OutStream& out, const vec4& vec)
		{
			return out << vec.x << " " << vec.y << " " << vec.z << " " << vec.w;
		}

		template<typename InStream>
		friend InStream& operator>> (InStream& in, vec4& vec)
		{
			return in >> vec.x >> vec.y >> vec.z >> vec.w;
		}
	};

	struct int3
	{
		int v1, v2, v3;
	};

	typedef unsigned long long uid;
	extern AVSTREAM_API uid GenerateUid();

	/*!
	 * Result type.
	 */
	struct Result
	{
		/*! Result code enumeration. */
		enum Code
		{
			OK = 0,
			Node_InvalidSlot,
			Node_InvalidLink,
			Node_NotLinked,
			Node_Incompatible,
			Node_NotConfigured,
			Node_NotSupported,
			Node_AlreadyConfigured,
			Node_InvalidConfiguration,
			Node_InvalidInput,
			Node_InvalidOutput,
			Node_LinkFailed,
			Decoder_NoSuitableBackendFound,
			Encoder_NoSuitableBackendFound,
			Encoder_IncompleteFrame,
			Encoder_SurfaceAlreadyRegistered,
			Encoder_SurfaceNotRegistered,
			Decoder_SurfaceAlreadyRegistered,
			Decoder_SurfaceNotRegistered,
			Surface_InvalidBackend,
			IO_Retry,
			IO_Empty,
			IO_Full,
			IO_OutOfMemory,
			IO_InvalidArgument,
			File_OpenFailed,
			File_ReadFailed,
			File_WriteFailed,
			File_EOF,
			Pipeline_Stop,
			Pipeline_AlreadyProfiling,
			Pipeline_NotProfiling,
			Network_BindFailed,
			Network_ResolveFailed,
			Network_SendFailed,
			Network_RecvFailed,
			EncoderBackend_NoSuitableCodecFound,
			EncoderBackend_InitFailed,
			EncoderBackend_ReconfigFailed,
			EncoderBackend_ShutdownFailed,
			EncoderBackend_NotInitialized,
			EncoderBackend_InvalidDevice,
			EncoderBackend_InvalidSurface,
			EncoderBackend_SurfaceNotRegistered,
			EncoderBackend_SurfaceAlreadyRegistered,
			EncoderBackend_OutOfMemory,
			EncoderBackend_FlushError,
			EncoderBackend_MapFailed,
			EncoderBackend_UnmapFailed,
			EncoderBackend_EncodeFailed,
			EncoderBackend_CapabilityCheckFailed,
			DecoderBackend_InitFailed,
			DecoderBackend_ReconfigFailed,
			DecoderBackend_InvalidDevice,
			DecoderBackend_InvalidParam,
			DecoderBackend_InvalidConfiguration,
			DecoderBackend_CodecNotSupported,
			DecoderBackend_NotInitialized,
			DecoderBackend_InvalidSurface,
			DecoderBackend_InvalidPayload,
			DecoderBackend_SurfaceNotRegistered,
			DecoderBackend_SurfaceAlreadyRegistered,
			DecoderBackend_ParseFailed,
			DecoderBackend_DecodeFailed,
			DecoderBackend_DisplayFailed,
			DecoderBackend_ShutdownFailed,
			DecoderBackend_ReadyToDisplay,
			DecoderBackend_PayloadIsExtraData,
			DecoderBackend_CapabilityCheckFailed,
			GeometryEncoder_Incomplete,
			GeometryEncoder_InvalidPayload,
			GeometryDecoder_Incomplete,
			GeometryDecoder_InvalidPayload,
			GeometryDecoder_InvalidBufferSize,
			GeometryDecoder_ClientRendererError,
			Node_NotReady,
			UnknownError,
			Num_ResultCode,
		};

		Result(Code code) : m_code(code)
		{}
		//! if(Result) returns true only if m_code == OK i.e. is ZERO.
		operator bool() const { return m_code == OK; }
		operator Code() const { return m_code; }
	private:
		Code m_code;
	};

	/*! Log severity class. */
	enum class LogSeverity
	{
		Never = 0,
		Debug,
		Info,
		Warning,
		Error,
		Critical,
		Num_LogSeverity,
	};

	/*! Graphics API device handle type. */
	enum class DeviceType
	{
		Invalid = 0, /*!< Invalid (null) device. */
		Direct3D11 = 1,     /*!< Direct3D 11 device. */
		Direct3D12 = 2,	/*!<Direct3D 12 device */
		OpenGL = 3,      /*!< OpenGL device (implicit: guarantees that OpenGL context is current in calling thread). */
		Vulkan = 4,	/*!<Vulkan device */
	};

	/*! Video codec. */
	enum class VideoCodec
	{
		Any = 0,
		Invalid = 0,
		H264, /*!< H264 */
		HEVC /*!< HEVC (H265) */
	};

	/*! Video encoding preset. */
	enum class VideoPreset
	{
		Default = 0,     /*!< Default encoder preset. */
		HighPerformance, /*!< High performance preset (potentially faster). */
		HighQuality,     /*!< High quality preset (potentially slower). */
	};

	/*! Video payload type. */
	enum class VideoPayloadType : uint8_t
	{
		FirstVCL = 0,    /*!< Video Coding Layer unit (first VCL in an access unit). */
		VCL,             /*!< Video Coding Layer unit (any subsequent in each access unit). */
		VPS,             /*!< Video Parameter Set (HEVC only) */
		SPS,             /*!< Sequence Parameter Set */
		PPS,             /*!< Picture Parameter Set */
		OtherNALUnit,    /*!< Other NAL unit. */
		AccessUnit,      /*!< Entire access unit (possibly multiple NAL units). */
		ExtraData		 /*!< Data containing info relating to the video */
	};

	enum class VideoExtraDataType : uint8_t
	{
		CameraTransform = 0
	};

	enum class NetworkDataType : uint8_t
	{
		H264 = 0,
		HEVC,
		Geometry
	};

	enum class GeometryPayloadType : uint8_t
	{
		Mesh = 1,
		Material,
		MaterialInstance,
		Texture,
		Animation,
		Node
	};

	enum class NodeDataType : uint8_t
	{
		Mesh = 0,
		Camera,
		Scene,
		ShadowMap,
		Hand,
		Light
	};

	enum class ActorStatus : uint8_t
	{
		Unknown = 0,
		Drawn,
		WantToRelease,
		Released
	};

	enum class AxesStandard:uint8_t
	{
		NotInitialized = 0,
		RightHanded=1,
		LeftHanded=2,
		YVertical=4,
		EngineeringStyle = 8|RightHanded,
		GlStyle=16|RightHanded,
		UnrealStyle=32|LeftHanded,
		UnityStyle=64|LeftHanded|YVertical,
	};
	inline AxesStandard operator|(const AxesStandard &a, const AxesStandard &b)
	{
		return (AxesStandard)((uint8_t)a | (uint8_t)b);
	}
	inline AxesStandard operator&(const AxesStandard &a, const AxesStandard &b)
	{
		return (AxesStandard)((uint8_t)a & (uint8_t)b);
	}
	struct DisplayInfo
	{
		uint32_t width;
		uint32_t height;
	};
	struct Handshake
	{
		DisplayInfo startDisplayInfo = DisplayInfo();
		float MetresPerUnit = 1.0f;
		float FOV = 90.0f;
		uint32_t udpBufferSize=0;			// In kilobytes.
		uint32_t maxBandwidthKpS=0;			// In kilobytes per second
		AxesStandard axesStandard = AxesStandard::NotInitialized;
		uint8_t framerate = 0;				// In hertz
		bool usingHands = false; //Whether to send the hand actors to the client.
		bool isVR = true;
		uint64_t resourceCount = 0; //Amount of resources the client has, and are appended to the handshake.
	};

	struct InputState
	{
		uint32_t buttonsPressed;
		uint32_t buttonsReleased;
		float trackpadAxisX;
		float trackpadAxisY;
		float joystickAxisX;
		float joystickAxisY;
	};

	struct HeadPose
	{
		avs::vec4 orientation;
		avs::vec3 position;
	};

	//Contains information to update the transform of a node.
	struct MovementUpdate
	{
		std::uint64_t timestamp;
		bool isGlobal = true;

		uid nodeID;
		vec3 position;
		vec4 rotation;
		
		vec3 velocity;
		vec3 angularVelocityAxis;
		float angularVelocityAngle;
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
		RPCH_NumChannels
	};

	enum class ClientMessagePayloadType : uint8_t
	{
		Invalid = 0,
		ActorStatus,
		ReceivedResources,
		ControllerPoses
	};
	struct ClientMessage
	{
		ClientMessage(ClientMessagePayloadType t) : clientMessagePayloadType(t) {}
		ClientMessagePayloadType clientMessagePayloadType;
	};
	
	//Message info struct containing how many actors have changed to what state; sent alongside two list of actor UIDs.
	struct ActorStatusMessage: public ClientMessage
	{
		size_t actorsDrawnAmount;
		size_t actorsWantToReleaseAmount;

		ActorStatusMessage()
			:ActorStatusMessage(0, 0)
		{}

		ActorStatusMessage(size_t actorsDrawnAmount, size_t actorsWantToReleaseAmount)
			:ClientMessage(ClientMessagePayloadType::ActorStatus),
			actorsDrawnAmount(actorsDrawnAmount),
			actorsWantToReleaseAmount(actorsWantToReleaseAmount)
		{}
	};

	//Message info struct containing how many resources were received; sent alongside a list of UIDs.
	struct ReceivedResourcesMessage: public ClientMessage
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
	struct ControllerPosesMessage: public ClientMessage
	{
		HeadPose headPose;
		HeadPose controllerPoses[2];

		ControllerPosesMessage()
		:ClientMessage(ClientMessagePayloadType::ControllerPoses)
		{}
	};

	enum class CommandPayloadType
	{
		Invalid,
		Shutdown,
		Setup,
		ActorBounds,
		AcknowledgeHandshake,
		SetPosition,
		UpdateActorMovement,
		ReconfigureVideo
	};
	struct Command
	{
		Command(CommandPayloadType t) : commandPayloadType(t) {}
		CommandPayloadType commandPayloadType ;
	};

	struct AcknowledgeHandshakeCommand : public Command
	{
		AcknowledgeHandshakeCommand() : Command(CommandPayloadType::AcknowledgeHandshake) {}
		AcknowledgeHandshakeCommand(size_t visibleActorAmount) : Command(CommandPayloadType::AcknowledgeHandshake), visibleActorAmount(visibleActorAmount){}

		size_t visibleActorAmount = 0; //Amount of visible actor IDs appended to the command payload.
	};

	struct SetPositionCommand : public Command
	{
		SetPositionCommand() : Command(CommandPayloadType::SetPosition) {}
		vec3 position;
	};
	
	struct VideoConfig
	{
		uint32_t	video_width = 0;
		uint32_t	video_height = 0;
		uint32_t	depth_width = 0;
		uint32_t	depth_height = 0;
		float perspectiveFOV = 110;
		uint32_t    use_10_bit_decoding = 0;
		uint32_t    use_yuv_444_decoding = 0;
		uint32_t	colour_cubemap_size = 0;
		int32_t		compose_cube = 0;
		int32_t     use_cubemap = 1;
		avs::VideoCodec videoCodec = avs::VideoCodec::Any;
	};

	struct SetupCommand : public Command
	{
		SetupCommand() : Command(CommandPayloadType::Setup) {}
		int32_t		port=0;
		uint32_t	debug_stream = 0;
		uint32_t 	do_checksums=0;
		uint32_t 	debug_network_packets=0;
		int32_t		requiredLatencyMs=0;
		avs::uid	server_id = 0;
		avs::AxesStandard axesStandard = avs::AxesStandard::NotInitialized;
		VideoConfig video_config;
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

	struct ActorBoundsCommand: public Command
	{
		size_t actorsShowAmount;
		size_t actorsHideAmount;

		ActorBoundsCommand()
			:ActorBoundsCommand(0, 0)
		{}

		ActorBoundsCommand(size_t actorsShowAmount, size_t actorsHideAmount)
			:Command(CommandPayloadType::ActorBounds), actorsShowAmount(actorsShowAmount), actorsHideAmount(actorsHideAmount)
		{}
	};

	struct UpdateActorMovementCommand : public Command
	{
		size_t updatesAmount;

		UpdateActorMovementCommand()
			:UpdateActorMovementCommand(0)
		{}

		UpdateActorMovementCommand(size_t updatesAmount)
			:Command(CommandPayloadType::UpdateActorMovement), updatesAmount(updatesAmount)
		{}
	};

	inline size_t GetCommandSize(CommandPayloadType t)
	{
		switch (t)
		{
		case CommandPayloadType::Setup:
			return sizeof(SetupCommand);
		case CommandPayloadType::Shutdown:
			return sizeof(ShutdownCommand);
		case CommandPayloadType::ActorBounds:
			return sizeof(ActorBoundsCommand);
		case CommandPayloadType::AcknowledgeHandshake:
			return sizeof(AcknowledgeHandshakeCommand);
		case CommandPayloadType::SetPosition:
			return sizeof(SetPositionCommand);
		case CommandPayloadType::UpdateActorMovement:
			return sizeof(UpdateActorMovementCommand);
		default:
			return 0;
		};
	}

	inline size_t GetClientMessageSize(ClientMessagePayloadType t)
	{
		switch (t)
		{
		case ClientMessagePayloadType::ActorStatus:
			return sizeof(ActorStatusMessage);
		case ClientMessagePayloadType::ReceivedResources:
			return sizeof(ReceivedResourcesMessage);
			case ClientMessagePayloadType::ControllerPoses:
				return sizeof(ControllerPosesMessage);
		default:
			return 0;
		};
	}
	/*! Graphics API device handle. */
	struct DeviceHandle
	{
		DeviceType type; /*!< Device handle type. */
		void* handle;    /*!< Native device handle. */

		operator bool() const
		{
			return handle != nullptr;
		}
		template<typename T> T* as() const
		{
			return reinterpret_cast<T*>(handle);
		}
	};
	struct NetworkFrame
	{
		uint64_t pts = UINT64_MAX;
		uint64_t dts = UINT64_MAX;
		size_t bufferSize = 0;
		bool broken = false; // True if any fragment of the data has been lost
		std::vector<uint8_t> buffer;
	};
} // avs
