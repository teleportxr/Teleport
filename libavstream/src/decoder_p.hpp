// libavstream
// (c) Copyright 2018-2024 Teleport XR Ltd

#pragma once

#include <memory>
#include <vector>
#include <queue>

#include "common_p.hpp"
#include "node_p.hpp"
#include <libavstream/decoder.hpp>

#include <parsers/nalu_parser_h264.hpp>
#include <parsers/nalu_parser_h265.hpp>



namespace avs
{

	struct Decoder::Private final : public PipelineNode::Private
	{
		AVSTREAM_PRIVATEINTERFACE(Decoder, PipelineNode)

		bool m_doChecksums=false;
	};

} // avs