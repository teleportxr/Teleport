// libavstream
// (c) Copyright 2018-2019 Simul Software Ltd

#pragma once

#include <cassert>
#include <cstdint>
#include <libavstream/abi.hpp>
#include <vector>

#define LIBAVSTREAM_VERSION 1

namespace avs
{
#pragma pack(push)
#pragma pack(1)
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
	enum class VideoCodec : uint8_t
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
		Invalid=0, 
		Mesh,
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
		Invalid=0,
		None,
		Mesh,
		Light,
		Bone
	};

	enum class NodeDataSubtype : uint8_t
	{
		Invalid=0,
		None,
		Body,
		LeftHand,
		RightHand
	};

	struct DisplayInfo
	{
		uint32_t width;
		uint32_t height;
	};

	struct VideoConfig
	{
		uint32_t	video_width = 0;
		uint32_t	video_height = 0;
		uint32_t	depth_width = 0;
		uint32_t	depth_height = 0;
		uint32_t	perspective_width = 0;
		uint32_t	perspective_height = 0;
		float       perspective_fov = 110;
		float       nearClipPlane = 0.5f;
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
		int32_t		specular_mips=0;
		int32_t		diffuse_x=0;
		int32_t		diffuse_y=0;
		int32_t		diffuse_cubemap_size=0;
		int32_t		light_x=0;
		int32_t		light_y=0;
		int32_t		light_cubemap_size=0;
		
		int32_t		shadowmap_x=0;
		int32_t		shadowmap_y=0;
		int32_t		shadowmap_size=0;
		float       draw_distance = 5;
	};

	struct AudioConfig
	{
		uint32_t sampleRate = 44100;
		uint32_t bitsPerSample = 16;
		uint32_t numChannels = 2;
	};

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
#pragma pack(pop)
} // avs
