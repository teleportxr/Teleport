// libavstream
// (c) Copyright 2018-2019 Simul Software Ltd

#pragma once

#include <vector>
#include <fstream>

#include <common_p.hpp>
#include <platform.hpp>
#include <libavstream/pipeline.hpp>

namespace avs
{
	struct Pipeline::Private
	{
		AVSTREAM_PRIVATEINTERFACE_BASE(Pipeline)
		std::vector<Node*> m_nodes;
		Timestamp m_startPlatformTimestamp;
		uint64_t m_lastTimestamp;
		uint64_t m_startTimestamp;
		bool m_started = false;

		std::ofstream m_statFile;
		void writeTimingsHeader();
		void writeTimings(uint32_t timestamp, const std::vector<double>& timings);

		Result process();
	};
}