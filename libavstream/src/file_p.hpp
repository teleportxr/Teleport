// libavstream
// (c) Copyright 2018-2019 Simul Software Ltd

#pragma once

#include <fstream>
#include <string>

#include <common_p.hpp>
#include <node_p.hpp>
#include <libavstream/file.hpp>

namespace avs
{

	struct File::Private final : public PipelineNode::Private
	{
		AVSTREAM_PRIVATEINTERFACE(File, PipelineNode)
		std::fstream m_file;
		std::string m_filename;
		FileAccess m_access = FileAccess::None;
	};

} // avs