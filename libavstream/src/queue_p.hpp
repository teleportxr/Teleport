// libavstream
// (c) Copyright 2018-2019 Simul Software Ltd

#pragma once

#include <vector>
#include <queue>
#include <mutex>

#include <common_p.hpp>
#include <node_p.hpp>
#include <libavstream/queue.hpp>

namespace avs
{

	struct Queue::Private final : public Node::Private
	{
		std::string name;
		AVSTREAM_PRIVATEINTERFACE(Queue, Node)
		std::queue<std::vector<char>> m_buffers;
		std::mutex m_mutex;
		size_t m_maxBuffers = 1;
		void flushInternal();
	};

} // avs