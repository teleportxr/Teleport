// libavstream
// (c) Copyright 2018-2024 Simul Software Ltd

#include <iostream>
#include "libavstream/network/networksource.h"

using namespace avs;

NetworkSource::NetworkSource(PipelineNode::Private* d_ptr)
	: PipelineNode(d_ptr)
{
}
