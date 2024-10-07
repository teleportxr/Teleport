// libavstream
// (c) Copyright 2018-2024 Teleport XR Ltd

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