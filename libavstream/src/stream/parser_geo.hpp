// libavstream
// (c) Copyright 2018-2019 Simul Software Ltd

#pragma once

#include <array>
#include <vector>

#include <libavstream/stream/parser_interface.hpp>

namespace avs
{
	class GeometryStreamParser final : public StreamParserInterface
	{
	public:
		GeometryStreamParser();

		Result configure(Node* node, OnPacketFn callback, uint32_t inputNodeIndex) override;
		Result parse(const char* buffer, size_t bufferSize) override;
		Result flush() override;

	private:
		Node* m_node;
		OnPacketFn m_callback;
		uint32_t m_inputNodeIndex;
	};

} // avs