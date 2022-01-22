// libavstream
// (c) Copyright 2018-2022 Simul Software Ltd

#pragma once

#include <cassert>
#include <cstdint>
#include <libavstream/abi.hpp>
#include <vector>
#include <mutex>
#include <queue>

#define LIBAVSTREAM_VERSION 1

#if defined(__GNUC__) || defined(__clang__)
#define AVS_PACKED __attribute__ ((packed))
#else
#define AVS_PACKED
#endif

namespace avs
{
#ifdef _MSC_VER
#pragma pack(push, 1)
#endif
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
			Network_NoConnection,
			Network_Disconnection,
			Network_PollTimeout,
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
			DecoderBackend_InvalidOutoutFormat,
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
			NetworkSink_SendingDataFailed,
			NetworkSink_PackingDataFailed,
			NetworkSink_InvalidStreamDataType,
			Failed,
			Num_ResultCode,
			HTTPUtil_NotInitialized,
			HTTPUtil_AlreadyInitialized,
			HTTPUtil_InitError,
			HTTPUtil_TransferError,
			NotSupported
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
		ALE,			 /*!< Custom name. NAL unit with alpha layer encoding metadata (HEVC only). */
		OtherNALUnit,    /*!< Other NAL unit. */
		AccessUnit      /*!< Entire access unit (possibly multiple NAL units). */
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
		Audio,
		VideoTagData
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
		Skin,
		Bone
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

	//! Identifies special use-cases for a locally-controlled node, used in UpdateNodeSubtypeCommand.
	enum class NodeSubtype : uint8_t
	{
		None=0,
		Body,
		LeftHand,
		RightHand
	};

	struct DisplayInfo
	{
		uint32_t width;
		uint32_t height;
	} AVS_PACKED;

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
		uint32_t webcam_width = 0;
		uint32_t webcam_height = 0;
		int32_t webcam_offset_x = 0;
		int32_t webcam_offset_y = 0;
		uint32_t    use_10_bit_decoding = 0;
		uint32_t    use_yuv_444_decoding = 0;
		uint32_t    use_alpha_layer_decoding = 1;
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
	} AVS_PACKED;


	struct AudioConfig
	{
		uint32_t sampleRate = 44100;
		uint32_t bitsPerSample = 16;
		uint32_t numChannels = 2;
	} AVS_PACKED;

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
	} AVS_PACKED;

	enum class FilePayloadType : uint8_t
	{
		Texture = 0,
		Mesh,
		Material
	};

	struct HTTPPayloadRequest
	{
		FilePayloadType type;
		std::string fileName;
	};

	enum class PayloadInfoType : uint8_t
	{
		Stream = 0,
		File
	};

	struct PayloadInfo
	{
		size_t dataSize = 0;
		PayloadInfoType payloadInfoType;

		PayloadInfo(PayloadInfoType inPayloadInfoType)
			: payloadInfoType(inPayloadInfoType) {}
		
	} AVS_PACKED;

	struct StreamPayloadInfo : PayloadInfo
	{
		uint64_t frameID = UINT64_MAX;
		double connectionTime = 0.0;
		bool broken = false; 

		StreamPayloadInfo() : PayloadInfo(PayloadInfoType::Stream) {}

	} AVS_PACKED;

	struct FilePayloadInfo : PayloadInfo
	{
		FilePayloadType httpPayloadType;

		FilePayloadInfo() : PayloadInfo(PayloadInfoType::File) {}
	} AVS_PACKED;

#ifdef _MSC_VER
#pragma pack(pop)
#endif

	template<class T>
	class ThreadSafeQueue
	{
	public:
		void push(T& val)
		{
			std::lock_guard<std::mutex> lock(m_mutex);
			m_data.push(val);
		}

		void push(T&& val)
		{
			std::lock_guard<std::mutex> lock(m_mutex);
			m_data.push(std::move(val));
		}

		void pop() noexcept
		{
			std::lock_guard<std::mutex> lock(m_mutex);
			m_data.pop();
		}

		T& front() noexcept
		{
			std::lock_guard<std::mutex> lock(m_mutex);
			return m_data.front();
		}

		T& back() noexcept
		{
			std::lock_guard<std::mutex> lock(m_mutex);
			return m_data.back();
		}

		bool empty() noexcept
		{
			std::lock_guard<std::mutex> lock(m_mutex);
			return m_data.empty();
		}

		size_t size() noexcept
		{
			std::lock_guard<std::mutex> lock(m_mutex);
			return m_data.size();
		}

		void clear() noexcept
		{
			std::lock_guard<std::mutex> lock(m_mutex);
			while (!m_data.empty())
			{
				m_data.pop();
			}
		}

		template <class... _Valty>
		T& emplace(_Valty&&... _Val)
		{
			std::lock_guard<std::mutex> guard(m_mutex);
#if _HAS_CXX17
			return m_data.emplace(std::forward<_Valty>(_Val)...);
#else // ^^^ C++17 or newer / C++14 vvv
			m_data.emplace(std::forward<_Valty>(_Val)...);
			return m_data.back();
#endif // _HAS_CXX17
		}

	private:
		std::mutex m_mutex;
		std::queue<T> m_data;
	};
} // avs
