// libavstream
// (c) Copyright 2018-2019 Simul Software Ltd

#include <pipeline_p.hpp>
#include <node_p.hpp>

using namespace avs;

Pipeline::Pipeline()
	: m_d(new Pipeline::Private(this))
{
	m_data = (Pipeline::Private*)m_d;
	reset();

}

Pipeline::~Pipeline()
{
	delete m_d;
}

uint64_t Pipeline::GetStartTimestamp() const
{
	return m_data->m_startTimestamp;
}

uint64_t Pipeline::GetTimestamp() const
{
	return m_data->m_lastTimestamp;
}

void Pipeline::add(Node* node)
{
	if (std::find(std::begin(m_data->m_nodes), std::end(m_data->m_nodes), node) == m_data->m_nodes.end())
	{
		m_data->m_nodes.push_back(node);
	}
	else
	{
		//AVSLOG(Warning) << "Pipeline: trying to add node that's already in this pipeline.\n";
		// Not a problem.
	}
}

void Pipeline::add(const std::initializer_list<Node*>& nodes)
{
	for (Node* node : nodes)
	{
		add(node);
	}
}

Result Pipeline::link(const std::initializer_list<Node*>& nodes)
{
	Node* prevNode = nullptr;
	for (Node* node : nodes)
	{
		if (prevNode)
		{
			if (Result result = Node::link(*prevNode, *node); !result)
			{
				return result;
			}
		}
		prevNode = node;
	}
	add(nodes);
	return Result::OK;
}

Node* Pipeline::front() const
{
	if (m_data->m_nodes.size() == 0)
	{
		return nullptr;
	}
	return m_data->m_nodes.front();
}

Node* Pipeline::back() const
{
	if (m_data->m_nodes.size() == 0)
	{
		return nullptr;
	}
	return m_data->m_nodes.back();
}

size_t Pipeline::getLength() const
{
	return m_data->m_nodes.size();
}

Result Pipeline::process()
{
	return m_data->process();
}

Result Pipeline::Private::process()
{
	// Returns in nanoseconds. Convert to milliseconds
	const uint64_t timestamp = (uint64_t)(0.001 * Platform::getTimeElapsed(m_startPlatformTimestamp, Platform::getTimestamp()));
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
		Node* node = m_nodes[index];
		assert(node);
		Timestamp profileStartTimestamp;
		if (isProfiling)
		{
			profileStartTimestamp = Platform::getTimestamp();
		}
		result = node->process(timestamp, deltaTime);
		if (isProfiling)
		{
			const Timestamp profileEndTimestamp = Platform::getTimestamp();
			timings[index] = Platform::getTimeElapsed(profileStartTimestamp, profileEndTimestamp);
		}
		if (!result && result != Result::IO_Empty)
		{
			break;
		}
	}
	if (isProfiling)
	{
		writeTimings(timestamp, timings);
	}
	return result;
}

void Pipeline::deconfigure()
{
	for (Node* node : m_data->m_nodes)
	{
		assert(node);
		node->deconfigure();
	}
}

void Pipeline::reset()
{
	// TODO: Unlink only connections relevant to this pipeline.
	for (Node* node : m_data->m_nodes)
	{
		assert(node);
		node->unlinkAll();
	}
	m_data->m_nodes.clear();

	restart();
}

void Pipeline::restart()
{
	memset(&(m_data->m_startPlatformTimestamp),0,sizeof(Timestamp));
	m_data->m_startPlatformTimestamp= Platform::getTimestamp();
	m_data->m_lastTimestamp = 0;
	m_data->m_startTimestamp = 0;
	m_data->m_started = false;
}

Result Pipeline::startProfiling(const char* statFileName)
{
	if (m_data->m_statFile.is_open())
	{
		return Result::Pipeline_AlreadyProfiling;
	}

	m_data->m_statFile.open(statFileName, std::ios::trunc);
	if (!m_data->m_statFile)
	{
		return Result::File_OpenFailed;
	}

	m_data->writeTimingsHeader();
	return Result::OK;
}

Result Pipeline::stopProfiling()
{
	if (!m_data->m_statFile.is_open())
	{
		return Result::Pipeline_NotProfiling;
	}
	m_data->m_statFile.close();
	return Result::OK;
}

void Pipeline::Private::writeTimingsHeader()
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

void Pipeline::Private::writeTimings(uint32_t timestamp, const std::vector<double>& timings)
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
