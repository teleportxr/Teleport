// libavstream
// (c) Copyright 2018-2022 Simul Software Ltd
#include <memory>
#include <vector>

#include "common_p.hpp"
#include "node_p.hpp"
#include "profiling.h"
#include <libavstream/genericdecoder.h>

namespace avs
{
	struct GenericDecoder::Private final : public PipelineNode::Private
	{
		AVSTREAM_PRIVATEINTERFACE(GenericDecoder, PipelineNode)
	};
} // avs

using namespace avs;

GenericDecoder::GenericDecoder()
	: PipelineNode(new GenericDecoder::Private(this))
{
	setNumSlots(1, 1);
}

GenericDecoder::~GenericDecoder()
{
	deconfigure();
}

Result GenericDecoder::configure(GenericTargetInterface* t,const char *n)
{
	if (m_configured)
	{
		Result deconf_result = deconfigure();
		if (deconf_result != Result::OK)
			return Result::Node_AlreadyConfigured;
	}
	name=n;
	m_target =t;

	m_configured = true;
	m_buffer.resize(2000);

	return Result::OK;
}

Result GenericDecoder::deconfigure()
{
	if (!m_configured)
	{
		return Result::Node_NotConfigured;
	}

	Result result = Result::OK;
	if (m_target)
	{
		unlinkOutput();
	}

	m_configured = false;

	return result;
}

Result GenericDecoder::process(uint64_t timestamp, uint64_t deltaTime)
{
	TELEPORT_PROFILE_AUTOZONE;
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
	// Process at most 100 messages, to prevent getting caught in an infinite loop as more
	// data arrives than we can process in the time.
	const size_t MAX_GENERIC_MESSAGES=100;
	for(size_t i=0;i<MAX_GENERIC_MESSAGES&&(result == Result::OK);i++)
	{
		size_t bufferSize = m_buffer.size();
		size_t bytesRead;
		result = input->read(this, m_buffer.data(), bufferSize, bytesRead);

		if (result == Result::IO_Empty)
		{
			break;
		}

		if (result == Result::IO_Retry)
		{
			m_buffer.resize(bufferSize);
			if(m_buffer.size()!=bufferSize)
				return Result::Failed;
			result = input->read(this, m_buffer.data(), bufferSize, bytesRead);
		}

		if (result != Result::OK)
		{
			AVSLOG(Warning) << "GenericDecoder: Failed to read input.";
			return result;
		}
		if (bytesRead < 1)
		{
			return Result::Failed;
		}
		uint8_t* ptr = m_buffer.data() ;
		// Drop the StreamPayloadInfo that the source node prepended.
		//ptr += sizeof(StreamPayloadInfo);
		//size_t sz = bytesRead - sizeof(StreamPayloadInfo);
		result = processPayload(ptr, bytesRead);
	} 
	if(result == Result::OK)
	{
		AVSLOGONCE(Warning) << "Processed max messages, must exit loop.\n";
		// TODO: Distinguish between unreliable (can drop messages) and reliable (should keep).
		input->drop();
	}

	return result;
}

Result GenericDecoder::processPayload(const uint8_t* buffer, size_t bufferSize)
{
	Result result = Result::UnknownError;

	if (m_target && bufferSize)
	{
		result = m_target->decode(buffer, bufferSize);
	}
	return result;
}

Result GenericDecoder::onInputLink(int slot, PipelineNode* node)
{
	if (!dynamic_cast<IOInterface*>(node))
	{
		AVSLOG(Error) << "GenericDecoder: Input node must provide data";
		return Result::Node_Incompatible;
	}
	return Result::OK;
}

Result GenericDecoder::onOutputLink(int slot, PipelineNode* node)
{
	if (!m_configured)
	{
		AVSLOG(Error) << "GenericDecoder: PipelineNode needs to be configured before it can accept output";
		return Result::Node_NotConfigured;
	}

	GenericTargetInterface* m = dynamic_cast<GenericTargetInterface*>(node);
	if (!m)
	{
		AVSLOG(Error) << "GenericDecoder: Output node is not a Mesh";
		return Result::Node_Incompatible;
	}
	return Result::OK;// m_backend->registerSurface(surface->getBackendSurface());
}

void GenericDecoder::onOutputUnlink(int slot, PipelineNode* node)
{
	if (!m_configured)
	{
		return;
	}

	GenericTargetInterface* m = dynamic_cast<GenericTargetInterface*>(node);
	if (m)
	{
		//m_backend->unregisterSurface(surface->getBackendSurface());
	}
}
