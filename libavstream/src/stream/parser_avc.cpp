// libavstream
// (c) Copyright 2018-2019 Simul Software Ltd

#include <stream/parser_avc.hpp>
#include <logger.hpp>

#include <parsers/nalu_parser_h264.hpp>
#include <parsers/nalu_parser_h265.hpp>

namespace avs
{
	StreamParserAVC::StreamParserAVC()
		: m_node(nullptr)
		, m_callback(nullptr)
		, m_inputNodeIndex(0)
	{
	}

	Result StreamParserAVC::configure(Node* node, OnPacketFn callback, uint32_t inputNodeIndex)
	{
		m_node = node;
		m_callback = callback;
		m_inputNodeIndex = inputNodeIndex;

		// The codex should be passed in later

		switch (m_codec)
		{
		case VideoCodec::H264:
			m_parser.reset(new NALUParser_H264);
			break;
		case VideoCodec::HEVC:
			m_parser.reset(new NALUParser_H265);
			break;
		default:
			assert(false);
		}

		return Result::OK;
	}

	Result StreamParserAVC::parse(const char* buffer, size_t bufferSize)
	{
		flush();

		size_t parseOffset = 0;
		
		for (; parseOffset < bufferSize; ++parseOffset)
		{
			if (m_aluCodeAccumulator.push(buffer[parseOffset]))
			{
				++parseOffset;
				break;
			}
		}

		size_t firstOffset = parseOffset;

		// We are at buffer[parseOffset]. We accumulate until EITHER we hit bufferSize OR we get another NAL block.
		for (; parseOffset < bufferSize; ++parseOffset)
		{
			const char byte = buffer[parseOffset];
			if (m_aluCodeAccumulator.push(byte))
			{ 
				Result callbackResult = Result::OK;
				
				bool isLastPayload;
				if (bufferSize - parseOffset <= m_aluCodeAccumulator.m_buffer.size())
				{
					isLastPayload = true;
				}
				else
				{
					isLastPayload = false;
				}

				size_t dataSize = parseOffset - m_aluCodeAccumulator.m_buffer.size() - firstOffset + 1;
				callbackResult = m_callback(m_node, m_inputNodeIndex, buffer, dataSize, firstOffset, isLastPayload);

				firstOffset = parseOffset + 1;

				if (!callbackResult)
				{
					return callbackResult;
				}
			}
		}

		Result result = Result::OK;
		if (parseOffset > firstOffset)
		{
			result = m_callback(m_node, m_inputNodeIndex, buffer, parseOffset - firstOffset, firstOffset, true);
		}

		return result;
	}

	Result StreamParserAVC::flush()
	{
		m_aluCodeAccumulator.reset();

		return Result::OK;
	}

} // avs