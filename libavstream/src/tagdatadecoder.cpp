// libavstream
// (c) Copyright 2018-2021 Simul Software Ltd

#include <libavstream/tagdatadecoder.hpp>
#include "common_p.hpp"
#include "node_p.hpp"
#include <libavstream/timer.hpp>

using namespace avs;

struct TagDataDecoder::Private final : public PipelineNode::Private
{
	AVSTREAM_PRIVATEINTERFACE(TagDataDecoder, PipelineNode)

	uint8_t m_streamId = 0;
};

TagDataDecoder::TagDataDecoder()
	: PipelineNode(new TagDataDecoder::Private(this))
{
	setNumSlots(1, 0);
}

TagDataDecoder::~TagDataDecoder()
{
	deconfigure();
}

Result TagDataDecoder::configure(uint8_t streamId, std::function<void(const uint8_t * data, size_t dataSize)> onReceiveDataCallback)
{
	m_onReceiveDataCallback = onReceiveDataCallback;

	if (m_configured)
	{
		Result deconf_result = deconfigure();
		if (deconf_result != Result::OK)
			return Result::Node_AlreadyConfigured;
	}

	m_configured = true;
	d().m_streamId = streamId;
	m_frameBuffer.resize(200);

	return Result::OK;
}


Result TagDataDecoder::deconfigure()
{
	if (!m_configured)
	{
		return Result::Node_NotConfigured;
	}

	m_stats = {};
	m_configured = false;

	return  Result::OK;
}

Result TagDataDecoder::process(uint64_t timestamp, uint64_t deltaTime)
{
	if (!m_configured)
	{
		return Result::Node_NotConfigured;
	}

	IOInterface* input = dynamic_cast<IOInterface*>(getInput(0));
	if (!input)
	{
		return Result::Node_InvalidInput;
	}

	Result result = Result::OK;

	do
	{
		size_t bufferSize = m_frameBuffer.size();
		size_t bytesRead;
		result = input->read(this, m_frameBuffer.data(), bufferSize, bytesRead);

		if (result == Result::IO_Empty)
		{
			break;
		}

		if (result == Result::IO_Retry)
		{
			m_frameBuffer.resize(bufferSize);
			result = input->read(this, m_frameBuffer.data(), bufferSize, bytesRead);
		}

		if (result != Result::OK || bytesRead < sizeof(StreamPayloadInfo))
		{
			AVSLOG(Warning) << "TagDataDecoder: Failed to read input.";
			return result;
		}

		const StreamPayloadInfo& frame = *reinterpret_cast<const StreamPayloadInfo*>(m_frameBuffer.data());

		// Copy frame info 
		//memcpy(&frame, m_frameBuffer.data(), sizeof(NetworkFrameInfo));

		++m_stats.framesReceived;

		if (frame.connectionTime)
		{
			m_stats.framesReceivedPerSec = float(m_stats.framesReceived) / float(frame.connectionTime);
		}

		// Check if data was lost or corrupted
		if (frame.broken || frame.dataSize == 0||frame.dataSize>bufferSize)
		{
			continue;
		}

		m_onReceiveDataCallback(m_frameBuffer.data() + sizeof(StreamPayloadInfo), frame.dataSize);

	} while (result == Result::OK);
	
	return result;
}

Result TagDataDecoder::onInputLink(int slot, PipelineNode* node)
{
	if (!dynamic_cast<IOInterface*>(node))
	{
		AVSLOG(Error) << "TagDataDecoder: Input node must provide data";
		return Result::Node_Incompatible;
	}
	return Result::OK;
}

Result TagDataDecoder::onOutputLink(int slot, PipelineNode* node)
{
	if (!m_configured)
	{
		AVSLOG(Error) << "TagDataDecoder: PipelineNode needs to be configured before it can accept output";
		return Result::Node_NotConfigured;
	}
	
	return Result::OK;
}

void TagDataDecoder::onOutputUnlink(int slot, PipelineNode* node)
{
	if (!m_configured)
	{
		return;
	}
}

uint8_t TagDataDecoder::getStreamId() const
{
	return d().m_streamId;
}