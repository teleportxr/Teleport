// libavstream
// (c) Copyright 2018-2024 Teleport XR Ltd

#pragma once

#include <cassert>
#include <cstdint>
#include <libavstream/abi.hpp>
#include <libavstream/common_exports.h>
#include <vector>
#include <mutex>
#include <queue>

#define LIBAVSTREAM_VERSION 1

#if defined(__GNUC__) || defined(__clang__)
#define _isnanf isnan
#endif

#ifndef AVS_PACKED
	#if defined(__GNUC__) || defined(__clang__)
		#define AVS_PACKED __attribute__ ((packed,aligned(1)))
	#else
		#define AVS_PACKED
	#endif
#endif

namespace avs
{
#ifdef _MSC_VER
#pragma pack(push, 1)
#endif
	typedef uint64_t uid;
	extern AVSTREAM_API uid GenerateUid();
	extern AVSTREAM_API void ClaimUidRange(avs::uid last);
	
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


	inline const char *stringOf(GeometryPayloadType t)
	{
		switch(t)
		{
			case GeometryPayloadType::Mesh:				return "Mesh";
			case GeometryPayloadType::Material:			return "Material";
			case GeometryPayloadType::MaterialInstance:	return "MaterialInstance";
			case GeometryPayloadType::Texture:			return "Texture";
			case GeometryPayloadType::Animation:		return "Animation";
			case GeometryPayloadType::Node:				return "Node";
			case GeometryPayloadType::Skeleton:			return "Skeleton";
			case GeometryPayloadType::FontAtlas:		return "FontAtlas";
			case GeometryPayloadType::TextCanvas:		return "TextCanvas";
			case GeometryPayloadType::TexturePointer:	return "TexturePointer";
			default:
				return "Invalid";
		}
	}
	enum class AudioPayloadType : uint8_t
	{
		Capture = 0
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
	} AVS_PACKED;

	enum class FilePayloadType : uint8_t
	{
		Invalid=0,
		Texture,
		Mesh,
		Material
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
		FilePayloadType httpPayloadType=FilePayloadType::Invalid;
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
