// libavstream
// (c) Copyright 2018-2019 Simul Software Ltd

#pragma once

#include <memory>
#include <vector>
#include <queue>

#include <common_p.hpp>
#include <node_p.hpp>
#include <libavstream/decoder.hpp>

#include <parsers/nalu_parser_h264.hpp>
#include <parsers/nalu_parser_h265.hpp>

#include <libavstream/geometry/mesh_interface.hpp>



namespace avs
{

	struct Decoder::Private final : public Node::Private
	{
		AVSTREAM_PRIVATEINTERFACE(Decoder, Node)

		bool m_doChecksums=false;
	};

} // avs