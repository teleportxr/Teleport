// libavstream
// (c) Copyright 2018-2024 Teleport XR Ltd

#pragma once

#include <libavstream/common.hpp>
#include <libavstream/memory.hpp>

namespace avs
{
	/*! Bitstream parser type */
	enum class StreamParserType
	{
		AVC_AnnexB, /*<! AVC Annex B bitstream parser */
		Geometry,
		Audio,
		Custom,     /*<! Custom bitstream parser */
		None,
		Default = AVC_AnnexB, /*<! Default bitstream parser */
	};

	class PipelineNode;
	/*!
	 * Common stream parser interface.
	 *
	 * Stream parser parses raw bitstream into logical codec packets (usually NAL units).
	 */
	class AVSTREAM_API StreamParserInterface : public UseInternalAllocator
	{
	public:
		static StreamParserInterface *Create(StreamParserType StreamParser);
		/*!
		 * OnPacket function prototype (invoked periodically by the parser to notify about parsed packets in the stream).
		 * \param node PipelineNode instance passed in configure().
		 * \param buffer Pointer to the beginning of the overall data.
		 * \param dataSize Number of bytes in the data.
		 * \param dataOffset Offset into the buffer where the data begins.
		 * \return Result::OK or error code.
		 */
		typedef Result(*OnPacketFn)(PipelineNode* node, uint32_t inputNodeIndex, const char* buffer, size_t dataSize, size_t dataOffset, bool isLastPayload);

		virtual ~StreamParserInterface() = default;

		/*!
		 * Configure stream parser.
		 * \param node PipelineNode instance to pass to the callback invocation.
		 * \param callback Callback function invoked every time new codec packet has been fully parsed.
		 * \param inputNodeIndex Which input node this is the parser for.
		 * \return
		 *  - Result::OK on success.
		 */
		virtual Result configure(PipelineNode* node, OnPacketFn callback, uint32_t inputNodeIndex) = 0;

		/*!
		 * Parse bitstream.
		 * \param buffer Pointer to input bitstream data.
		 * \param bufferSize Number of bytes in input buffer.
		 * \return
		 *  - Result::OK on success.
		 *  - Any error returned by OnPacketFn callback.
		 */
		virtual Result parse(const char* buffer, size_t bufferSize) = 0;

		/*!
		 * Flush stream parser resetting its state and releasing any allocated resources.
		 * This function should be called at the end of parsing to ensure that OnPacketFn callback is called for the very last part of bitstream.
		 * \return
		 *  - Result::OK on success.
		 *  - Any error returned by OnPacketFn callback.
		 */
		virtual Result flush() = 0;
	};

} // avs