// libavstream
// (c) Copyright 2018-2024 Teleport XR Ltd

#pragma once

#include <cassert>
#include <cstdint>

#define LIBAVSTREAM_VERSION 1

#if defined(__GNUC__) || defined(__clang__)
#define _isnanf isnan
#endif

#include <libavstream/common_packing.h>
#ifdef _MSC_VER
#pragma pack(push, 1)
#endif

namespace avs
{
	typedef uint64_t uid;
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
		HEVC = 1,
		Framed = 2,
		Generic=3
	};
	enum class NodeDataType : uint8_t
	{
		Invalid=0,
		None,
		Mesh,
		Light,
		TextCanvas,
		SubScene,
		Skeleton,
		Link
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
		Skeleton,
		FontAtlas,
		TextCanvas,
		TexturePointer,
	};
	//! Features supported by a client.
	struct RenderingFeatures
	{
		bool normals=false;				//!< Whether normal maps are supported.
		bool ambientOcclusion=false;	//!< Whether ambient occlusion maps are supported.
	} AVS_PACKED;
	enum class StreamingConnectionState :uint8_t
	{
		UNINITIALIZED = 0,
		NEW_UNCONNECTED,
		CONNECTING,
		CONNECTED,
		DISCONNECTED,
		FAILED,
		CLOSED,
		ERROR_STATE
	};
	inline const char *stringOf(StreamingConnectionState state)
	{
		switch(state)
		{
			case StreamingConnectionState::NEW_UNCONNECTED	:return "NEW_UNCONNECTED";
			case StreamingConnectionState::CONNECTING		:return "CONNECTING";
			case StreamingConnectionState::CONNECTED		:return "CONNECTED";
			case StreamingConnectionState::DISCONNECTED		:return "DISCONNECTED";
			case StreamingConnectionState::FAILED			:return "FAILED";
			case StreamingConnectionState::CLOSED			:return "CLOSED";
			case StreamingConnectionState::ERROR_STATE		:return "ERROR_STATE";
			default:
			return "INVALID ";
		};
	};
	enum class ComponentType : uint32_t
	{
		FLOAT=0,
		DOUBLE,
		HALF,
		UINT,
		USHORT,
		UBYTE,
		INT,
		SHORT,
		BYTE,
	};
} // avs
#ifdef _MSC_VER
#pragma pack(pop)
#endif
