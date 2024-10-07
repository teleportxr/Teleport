// libavstream
// (c) Copyright 2018-2024 Teleport XR Ltd

#pragma once

#include <vector>
#include <mutex>

#include "common_p.hpp"
#include "node_p.hpp"
#include <libavstream/buffer.hpp>

namespace avs
{

struct Buffer::Private final : public PipelineNode::Private
{
	AVSTREAM_PRIVATEINTERFACE(Buffer, PipelineNode)
	std::vector<char> m_buffer;
	mutable std::mutex m_mutex;
	size_t m_readCursor = 0;
	size_t m_writeCursor = 0;
	size_t m_bytesAvailable = 0;
};

} // avs
