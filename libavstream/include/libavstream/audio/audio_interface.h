// libavstream
// (c) Copyright 2018-2024 Teleport XR Ltd

#pragma once
#include <vector>
#include <map>
#include <memory>
#include <libavstream/common.hpp>
#include <libavstream/memory.hpp>

namespace avs
{
	/*!
	 * Audio encoder parameters.
	 */
	struct AudioEncoderParams
	{
		AudioCodec codec = AudioCodec::Any;
	};

	/*!
	 * Audio encoder backend interface.
	 */
	class AVSTREAM_API AudioEncoderBackendInterface : public UseInternalAllocator
	{
	public:
		virtual ~AudioEncoderBackendInterface() = default;
		virtual Result initialize(const AudioEncoderParams& params) = 0;
		virtual Result encode(uint32_t timestamp, uint8_t* captureData, size_t captureDataSize) = 0;
		virtual Result mapOutputBuffer(void*& bufferPtr, size_t& bufferSizeInBytes) = 0;
		virtual Result unmapOutputBuffer() = 0;
		virtual Result shutdown() = 0;
	};

	/*!
	 * Audio target backend interface.
	 */
	class AVSTREAM_API AudioTargetBackendInterface : public UseInternalAllocator
	{
	public:
		virtual ~AudioTargetBackendInterface() = default;
		virtual Result process(const void* buffer, size_t bufferSizeInBytes, AudioPayloadType payloadType) = 0;
		virtual Result deconfigure() = 0;
	};


	/*!
	 * Audio parser interface.
	 */
	class AudioParserInterface
	{
	public:
		virtual ~AudioParserInterface() = default;
		virtual AudioPayloadType classify(const uint8_t* buffer, size_t bufferSize, size_t& dataOffset) const = 0;
		static constexpr size_t HeaderSize = 2;
	};
} // avs