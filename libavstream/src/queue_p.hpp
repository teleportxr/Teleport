// libavstream
// (c) Copyright 2018-2022 Simul Software Ltd

#pragma once

#include <mutex>

#include "common_p.hpp"
#include "node_p.hpp"
#include <libavstream/queue.hpp>

namespace avs
{

	struct Queue::Private final : public PipelineNode::Private
	{
		AVSTREAM_PRIVATEINTERFACE(Queue, PipelineNode)
	};

} // avs