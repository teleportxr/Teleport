// libavstream
// (c) Copyright 2018-2024 Simul Software Ltd

#include "node_p.hpp"
#include "libavstream/pipeline.hpp"
#include <algorithm>

using namespace avs;

Pipeline::Pipeline()
	: m_d(this)
{
	reset();

}

Pipeline::~Pipeline()
{
}

uint64_t Pipeline::GetStartTimestamp() const
{
	return m_startTimestamp;
}

uint64_t Pipeline::GetTimestamp() const
{
	return m_lastTimestamp;
}

void Pipeline::add(PipelineNode* node)
{
	#ifdef _MSC_VER
	if (std::find(std::begin(m_nodes), std::end(m_nodes), node) == m_nodes.end())
	#else
	for(auto n:m_nodes)
	{
		if(n==node)
			return;
	}
	#endif
	{
		m_nodes.push_back(node);
	}
	//else
	{
		//AVSLOG(Warning) << "Pipeline: trying to add node that's already in this pipeline.\n";
		// Not a problem.
	}
}

void Pipeline::add(const std::initializer_list<PipelineNode*>& nodes)
{
	for (PipelineNode* node : nodes)
	{
		add(node);
	}
}

Result Pipeline::link(const std::initializer_list<PipelineNode*>& nodes)
{
	PipelineNode* prevNode = nullptr;
	for (PipelineNode* node : nodes)
	{
		if (prevNode)
		{
			if (Result result = PipelineNode::link(*prevNode, *node); !result)
			{
				return result;
			}
		}
		prevNode = node;
	}
	add(nodes);
	return Result::OK;
}

PipelineNode* Pipeline::front() const
{
	if (m_nodes.size() == 0)
	{
		return nullptr;
	}
	return m_nodes.front();
}

PipelineNode* Pipeline::back() const
{
	if (m_nodes.size() == 0)
	{
		return nullptr;
	}
	return m_nodes.back();
}

size_t Pipeline::getLength() const
{
	return m_nodes.size();
}

void Pipeline::processAsync()
{
	if(!pipelineThread.joinable())
		pipelineThread = std::thread(&Pipeline::processAsyncFn, this);
	pipelineThreadActive = true;
}

void Pipeline::processAsyncFn()
{
	while (pipelineThreadActive)
	{
		Result result= process();
		if(!result)
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}
}

Result Pipeline::process()
{
	// Returns in milliseconds.
	const uint64_t timestamp = (uint64_t)(Platform::getTimeElapsedInMilliseconds(m_startPlatformTimestamp, Platform::getTimestamp()));
	if (!m_started)
	{
		m_startTimestamp = timestamp;
		m_lastTimestamp = timestamp;
		m_started = true;
	}
	const uint64_t deltaTime = timestamp - m_lastTimestamp;

	m_lastTimestamp = timestamp;
	
	const bool isProfiling = m_statFile.is_open();
	std::vector<double> timings;
	if (isProfiling)
	{
		timings.resize(m_nodes.size());
	}
	Result result = Result::OK;
	for (size_t index = 0; index < m_nodes.size(); ++index)
	{
		PipelineNode* node = m_nodes[index];
		assert(node);
		Timestamp profileStartTimestamp;
		if (isProfiling)
		{
			profileStartTimestamp = Platform::getTimestamp();
		}
		result = node->process(timestamp, deltaTime);
		node->setResult(result);
		if (isProfiling)
		{
			const Timestamp profileEndTimestamp = Platform::getTimestamp();
			timings[index] = Platform::getTimeElapsedInMilliseconds(profileStartTimestamp, profileEndTimestamp);
		}
		if (!result && result != Result::IO_Empty)
		{
			break;
		}
	}
	if (isProfiling)
	{
		writeTimings(uint32_t(timestamp), timings);
	}
	return result;
}

void Pipeline::deconfigure()
{
	pipelineThreadActive=false;
	if(pipelineThread.joinable())
		pipelineThread.join();
	for (PipelineNode* node : m_nodes)
	{
		assert(node);
		node->deconfigure();
	}
}

void Pipeline::reset()
{
	// TODO: Unlink only connections relevant to this pipeline.
	for (PipelineNode* node : m_nodes)
	{
		assert(node);
		node->unlinkAll();
	}
	m_nodes.clear();

	restart();
}

void Pipeline::restart()
{
	memset(&(m_startPlatformTimestamp),0,sizeof(Timestamp));
	m_startPlatformTimestamp= Platform::getTimestamp();
	m_lastTimestamp = 0;
	m_startTimestamp = 0;
	m_started = false;
}

Result Pipeline::startProfiling(const char* statFileName)
{
	if (m_statFile.is_open())
	{
		return Result::Pipeline_AlreadyProfiling;
	}

	m_statFile.open(statFileName, std::ios::trunc);
	if (!m_statFile)
	{
		return Result::File_OpenFailed;
	}

	writeTimingsHeader();
	return Result::OK;
}

Result Pipeline::stopProfiling()
{
	if (!m_statFile.is_open())
	{
		return Result::Pipeline_NotProfiling;
	}
	m_statFile.close();
	return Result::OK;
}

void Pipeline::writeTimingsHeader()
{
	assert(m_statFile.is_open());

	if (m_statFile)
	{
		m_statFile << "Time";
		for (size_t index = 0; index < m_nodes.size(); ++index)
		{
			m_statFile << "," << index << "_" << m_nodes[index]->getDisplayName();
		}
		m_statFile << ",Pipeline\n";
	}
}

void Pipeline::writeTimings(uint32_t timestamp, const std::vector<double>& timings)
{
	assert(m_statFile.is_open());

	if (m_statFile)
	{
		m_statFile << (timestamp / 1000.0);

		double pipelineTotalTime = 0.0;
		for (auto timing : timings)
		{
			m_statFile << "," << timing;
			pipelineTotalTime += timing;
		}

		m_statFile << "," << pipelineTotalTime << "\n";
	}
}
