// libavstream
// (c) Copyright 2018-2022 Simul Software Ltd

#pragma once

#include "common_p.hpp"
#include "node_p.hpp"
#include <libavstream/forwarder.hpp>

#include <vector>

namespace avs
{
	struct Forwarder::Private final : public PipelineNode::Private
	{
		AVSTREAM_PRIVATEINTERFACE(Forwarder, PipelineNode)
		std::vector<char> m_buffer;
		size_t m_chunkSize = 0;
		unsigned streamId=0;
	};
} // avs