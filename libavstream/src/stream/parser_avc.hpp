// libavstream
// (c) Copyright 2018-2019 Simul Software Ltd

#pragma once

#include <array>
#include <vector>

#include <libavstream/stream/parser_interface.hpp>
#include <libavstream/common.hpp>

#define SEND_EXTRA_DATA_SEPARATELY 0

namespace avs
{
	class StreamParserAVC final : public StreamParserInterface
	{
	public:
		StreamParserAVC();

		Result configure(Node* node, OnPacketFn callback, uint32_t inputNodeIndex) override;
		Result parse(const char* buffer, size_t bufferSize) override;
		Result flush() override;

	private:
		struct ALUCodeAccumulator
		{
			inline bool push(char byte)
			{
				m_buffer[0] = m_buffer[1];
				m_buffer[1] = m_buffer[2];
				m_buffer[2] = byte;
				return m_buffer[0] == 0 && m_buffer[1] == 0 && m_buffer[2] == 1;
			}
			inline void reset()
			{
				m_buffer[0] = 0xFF;
				m_buffer[1] = 0xFF;
				m_buffer[2] = 0xFF;
			}
			std::array<unsigned char, 3> m_buffer = { 0xFF, 0xFF, 0xFF };
		};

		ALUCodeAccumulator m_aluCodeAccumulator;
		
		Node* m_node;
		OnPacketFn m_callback;
		uint32_t m_inputNodeIndex;

		VideoCodec m_codec = VideoCodec::HEVC;
		std::unique_ptr<class NALUParserInterface> m_parser;
	};

} // avs