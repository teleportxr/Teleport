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
	enum class AxesStandard : uint8_t
	{
		NotInitialized = 0,
		RightHanded = 1,
		LeftHanded = 2,
		YVertical = 4,
		EngineeringStyle = 8 | RightHanded,
		GlStyle = 16 | RightHanded,
		UnrealStyle = 32 | LeftHanded,
		UnityStyle = 64 | LeftHanded | YVertical,
	};

	inline AxesStandard operator|(const AxesStandard& a, const AxesStandard& b)
	{
		return (AxesStandard)((uint8_t)a | (uint8_t)b);
	}

	inline AxesStandard operator&(const AxesStandard& a, const AxesStandard& b)
	{
		return (AxesStandard)((uint8_t)a & (uint8_t)b);
	}

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

		void operator=(const float* v)
		{
			x = v[0];
			y = v[1];
			z = v[2];
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

		vec4(const float* v)
		{
			this->x = v[0];
			this->y = v[1];
			this->z = v[2];
			this->w = v[3];
		}

		vec4 operator-() const
		{
			return vec4(-x, -y, -z, -w);
		}

		void operator=(const float* v)
		{
			x = v[0];
			y = v[1];
			z = v[2];
			w = v[3];
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

	inline vec2 operator*(float lhs, const vec2& rhs)
	{
		return vec2(rhs.x * lhs, rhs.y * lhs);
	}

	inline vec3 operator*(float lhs, const vec3& rhs)
	{
		return vec3(rhs.x * lhs, rhs.y * lhs, rhs.z * lhs);
	}

	inline vec4 operator*(float lhs, const vec4& rhs)
	{
		return vec4(rhs.x * lhs, rhs.y * lhs, rhs.z * lhs, rhs.w * lhs);
	}

	struct int3
	{
		int v1, v2, v3;
	};

	struct Mat4x4
	{
		//[Row, Column]
		float m00, m01, m02, m03;
		float m10, m11, m12, m13;
		float m20, m21, m22, m23;
		float m30, m31, m32, m33;

		Mat4x4()
			:m00(1.0f), m01(0.0f), m02(0.0f), m03(0.0f),
			m10(0.0f), m11(1.0f), m12(0.0f), m13(0.0f),
			m20(0.0f), m21(0.0f), m22(1.0f), m23(0.0f),
			m30(0.0f), m31(0.0f), m32(0.0f), m33(1.0f)
		{}

		Mat4x4(float m00, float m01, float m02, float m03,
			   float m10, float m11, float m12, float m13,
			   float m20, float m21, float m22, float m23,
			   float m30, float m31, float m32, float m33)
			:m00(m00), m01(m01), m02(m02), m03(m03),
			m10(m10), m11(m11), m12(m12), m13(m13),
			m20(m20), m21(m21), m22(m22), m23(m23),
			m30(m30), m31(m31), m32(m32), m33(m33)
		{}

		Mat4x4 operator*(const Mat4x4& rhs) const
		{
			return Mat4x4
			(
				m00 * rhs.m00 + m01 * rhs.m10 + m02 * rhs.m20 + m03 * rhs.m30, m00 * rhs.m01 + m01 * rhs.m11 + m02 * rhs.m21 + m03 * rhs.m31, m00 * rhs.m02 + m01 * rhs.m12 + m02 * rhs.m22 + m03 * rhs.m32, m00 * rhs.m03 + m01 * rhs.m13 + m02 * rhs.m23 + m03 * rhs.m33,
				m10 * rhs.m00 + m11 * rhs.m10 + m12 * rhs.m20 + m13 * rhs.m30, m10 * rhs.m01 + m11 * rhs.m11 + m12 * rhs.m21 + m13 * rhs.m31, m10 * rhs.m02 + m11 * rhs.m12 + m12 * rhs.m22 + m13 * rhs.m32, m10 * rhs.m03 + m11 * rhs.m13 + m12 * rhs.m23 + m13 * rhs.m33,
				m20 * rhs.m00 + m21 * rhs.m10 + m22 * rhs.m20 + m23 * rhs.m30, m20 * rhs.m01 + m21 * rhs.m11 + m22 * rhs.m21 + m23 * rhs.m31, m20 * rhs.m02 + m21 * rhs.m12 + m22 * rhs.m22 + m23 * rhs.m32, m20 * rhs.m03 + m21 * rhs.m13 + m22 * rhs.m23 + m23 * rhs.m33,
				m30 * rhs.m00 + m31 * rhs.m10 + m32 * rhs.m20 + m33 * rhs.m30, m30 * rhs.m01 + m31 * rhs.m11 + m32 * rhs.m21 + m33 * rhs.m31, m30 * rhs.m02 + m31 * rhs.m12 + m32 * rhs.m22 + m33 * rhs.m32, m30 * rhs.m03 + m31 * rhs.m13 + m32 * rhs.m23 + m33 * rhs.m33
			);
		}

		static Mat4x4 convertToStandard(const Mat4x4& matrix, avs::AxesStandard sourceStandard, avs::AxesStandard targetStandard)
		{
			
			Mat4x4 convertedMatrix = matrix;

			switch(sourceStandard)
			{
			case avs::AxesStandard::UnityStyle:
				switch(targetStandard)
				{
				case avs::AxesStandard::EngineeringStyle:
					///POSITION:
					//convertedMatrix.m03 = matrix.m03;
					convertedMatrix.m13 = matrix.m23;
					convertedMatrix.m23 = matrix.m13;

					//ROTATION (Implicitly handles scale.):
					//convertedMatrix.m00 = matrix.m00;
					convertedMatrix.m01 = matrix.m02;
					convertedMatrix.m02 = matrix.m01;

					convertedMatrix.m10 = matrix.m20;
					convertedMatrix.m11 = matrix.m22;
					convertedMatrix.m12 = matrix.m21;

					convertedMatrix.m20 = matrix.m10;
					convertedMatrix.m21 = matrix.m12;
					convertedMatrix.m22 = matrix.m11;

					break;
				case::avs::AxesStandard::GlStyle:
					convertedMatrix.m20 = -matrix.m20;
					convertedMatrix.m21 = -matrix.m21;
					convertedMatrix.m22 = -matrix.m22;
					convertedMatrix.m23 = -matrix.m23;

					break;
				default:
					//AVSLOG(Error) << "Unrecognised targetStandard in Mat4x4::convertToStandard!\n";
					break;
				}
				break;
			case avs::AxesStandard::UnrealStyle:
				switch(targetStandard)
				{
				case avs::AxesStandard::EngineeringStyle:
					///POSITION:
					convertedMatrix.m03 = matrix.m13;
					convertedMatrix.m13 = matrix.m03;
					//convertedMatrix.m23 = matrix.m23;

					//ROTATION (Implicitly handles scale.):
					convertedMatrix.m00 = matrix.m11;
					convertedMatrix.m01 = matrix.m10;
					convertedMatrix.m02 = matrix.m12;

					convertedMatrix.m10 = matrix.m01;
					convertedMatrix.m11 = matrix.m00;
					convertedMatrix.m12 = matrix.m02;

					convertedMatrix.m20 = matrix.m21;
					convertedMatrix.m21 = matrix.m20;
					//convertedMatrix.m22 = matrix.m22;

					break;
				case::avs::AxesStandard::GlStyle:
					//+position.y, +position.z, -position.x
					///POSITION:
					convertedMatrix.m03 = matrix.m13;
					convertedMatrix.m13 = matrix.m23;
					convertedMatrix.m23 = -matrix.m03;

					//ROTATION (Implicitly handles scale.):
					convertedMatrix.m00 = matrix.m11;
					convertedMatrix.m01 = matrix.m12;
					convertedMatrix.m02 = matrix.m10;

					convertedMatrix.m10 = matrix.m21;
					convertedMatrix.m11 = matrix.m22;
					convertedMatrix.m12 = matrix.m20;

					convertedMatrix.m20 = -matrix.m01;
					convertedMatrix.m21 = -matrix.m02;
					convertedMatrix.m22 = -matrix.m00;

					break;
				default:
					//AVSLOG(Error) << "Unrecognised targetStandard in Mat4x4::convertToStandard!\n";
					break;
				}
				break;
			default:
				//AVSLOG(Error) << "Unrecognised sourceStandard in Mat4x4::convertToStandard!\n";
				break;
			}

			return convertedMatrix;
		}
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
			Node_Null,
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
			AudioTarget_InvalidBackend,
			AudioTargetBackend_AudioProcessingError,
			AudioTargetBackend_NullAudioPlayer,
			AudioTargetBackend_PlayerDeconfigurationError,
			Node_NotReady,
			UnknownError,
			Num_ResultCode,
			NetworkSink_SendingDataFailed,
			NetworkSink_PackingDataFailed,
			NetworkSink_InvalidStreamDataType
		};

		Result(Code code) : m_code(code)
		{}
		//! if(Result) returns true only if m_code == OK i.e. is ZERO.
		operator bool() const { return m_code == OK; }
		operator Code() const { return m_code; }
		bool operator ==(const Code & c) const { return m_code == c; }
		bool operator !=(const Code & c) const { return m_code != c; }
		bool operator ==(const Result& r) const { return m_code == r.m_code; }
		bool operator !=(const Result& r) const { return m_code != r.m_code; }

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

	/*! Audio codec. */
	enum class AudioCodec
	{
		Any = 0,
		Invalid = 0,
		PCM
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
		Geometry,
		Audio
	};

	enum class GeometryPayloadType : uint8_t
	{
		Mesh = 1,
		Material,
		MaterialInstance,
		Texture,
		Animation,
		Node,
		Skin
	};

	enum class AudioPayloadType : uint8_t
	{
		Capture = 0
	};

	enum class NodeDataType : uint8_t
	{
		Mesh = 0,
		Camera,
		Scene,
		ShadowMap,
		Light,
		Bone
	};

	enum class NodeDataSubtype : uint8_t
	{
		None,
		Body,
		LeftHand,
		RightHand
	};

	enum class NodeStatus : uint8_t
	{
		Unknown = 0,
		Drawn,
		WantToRelease,
		Released
	};
	
	#pragma pack(push)
	#pragma pack(1)
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
		bool usingHands = false; //Whether to send the hand nodes to the client.
		bool isVR = true;
		uint64_t resourceCount = 0; //Amount of resources the client has, and are appended to the handshake.
	};
	
	enum InputEventType : unsigned char
	{
		None=0,
		Click
	};
	
	struct InputEvent
	{
		uint32_t eventId;		 //< A monotonically increasing event identifier.
		uid inputUid;			//< e.g. the uniqe identifier for this button or control.
		uint32_t intValue;
	};

	struct InputState
	{
		uint32_t controllerId;
		uint32_t buttonsDown;		// arbitrary bitfield.
		float trackpadAxisX;
		float trackpadAxisY;
		float joystickAxisX;
		float joystickAxisY;
		uint32_t numEvents;
	};
	#pragma pack(pop)

	struct Pose
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
		RPCH_Origin = 7,
		RPCH_NumChannels
	};

	enum class ClientMessagePayloadType : uint8_t
	{
		Invalid = 0,
		NodeStatus,
		ReceivedResources,
		ControllerPoses
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
		Pose headPose;
		Pose controllerPoses[2];

		ControllerPosesMessage()
		:ClientMessage(ClientMessagePayloadType::ControllerPoses)
		{}
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
	struct Command
	{
		Command(CommandPayloadType t) : commandPayloadType(t) {}
		CommandPayloadType commandPayloadType ;
	};

	struct AcknowledgeHandshakeCommand : public Command
	{
		AcknowledgeHandshakeCommand() : Command(CommandPayloadType::AcknowledgeHandshake) {}
		AcknowledgeHandshakeCommand(size_t visibleNodeAmount) : Command(CommandPayloadType::AcknowledgeHandshake), visibleNodeAmount(visibleNodeAmount){}

		size_t visibleNodeAmount = 0; //Amount of visible node IDs appended to the command payload.
	};

	struct SetPositionCommand : public Command
	{
		SetPositionCommand() : Command(CommandPayloadType::SetPosition) {}
		vec3 origin_pos;
		uint8_t set_relative_pos;
		vec3 relative_pos;
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
		int32_t     stream_webcam = 0;
		avs::VideoCodec videoCodec = avs::VideoCodec::Any;
		int32_t		specular_x=0;
		int32_t		specular_y=0;
		int32_t		specular_cubemap_size=0;
		int32_t		rough_x=0;
		int32_t		rough_y=0;
		int32_t		rough_cubemap_size=0;
		int32_t		diffuse_x=0;
		int32_t		diffuse_y=0;
		int32_t		diffuse_cubemap_size=0;
		int32_t		light_x=0;
		int32_t		light_y=0;
		int32_t		light_cubemap_size=0;
		
		int32_t		shadowmap_x=0;
		int32_t		shadowmap_y=0;
		int32_t		shadowmap_size=0;
	};

	struct AudioConfig
	{
		uint32_t sampleRate = 44100;
		uint32_t bitsPerSample = 16;
		uint32_t numChannels = 2;
	};

	struct SetupCommand : public Command
	{
		SetupCommand() : Command(CommandPayloadType::Setup) {}
		int32_t		port=0;
		uint32_t	debug_stream = 0;
		uint32_t 	do_checksums=0;
		uint32_t 	debug_network_packets=0;
		int32_t		requiredLatencyMs=0;
		uint32_t    idle_connection_timeout = 5000;
		avs::uid	server_id = 0;
		avs::AxesStandard axesStandard = avs::AxesStandard::NotInitialized;
		int32_t audio_input_enabled = 0;
		int32_t lock_player_height = 1;
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

	struct NodeBoundsCommand: public Command
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

	inline size_t GetCommandSize(CommandPayloadType t)
	{
		switch (t)
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
		switch (t)
		{
		case ClientMessagePayloadType::NodeStatus:
			return sizeof(NodeStatusMessage);
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
	struct NetworkFrameInfo
	{
		uint64_t pts = UINT64_MAX;
		uint64_t dts = UINT64_MAX;
		size_t dataSize = 0;
		bool broken = false; // True if any fragment of the data has been lost
	};
} // avs
