// libavstream
// (c) Copyright 2018-2024 Teleport XR Ltd
#include <memory>
#include <vector>

#include "common_p.hpp"
#include "node_p.hpp"
#include <libavstream/genericencoder.h>

#include <libavstream/buffer.hpp>
#include "profiling.h"
namespace avs
{
	struct GenericEncoder::Private final : public PipelineNode::Private
	{
		AVSTREAM_PRIVATEINTERFACE(GenericEncoder, PipelineNode)
		bool m_configured = false;
		bool m_outputPending = false;

		Result process(uint64_t timestamp, IOInterface *outputNode);

		Result writeOutput(IOInterface* outputNode);
		// Geom encoder doesn't own its backend.
		GenericEncoderBackendInterface *m_backend;
	};
} // avs

using namespace avs;

void ClientServerMessageStack::PushBuffer(std::shared_ptr<std::vector<uint8_t>> b)
{
	std::lock_guard l(mutex);
	if(buffers.size()>1000)
	{
		std::cerr<<"too many buffers in stack"<<std::endl;
		return;
	}
	buffers.push_back(b);
}

bool ClientServerMessageStack::mapOutputBuffer(void*& bufferPtr, size_t& bufferSizeInBytes)
{
	TELEPORT_PROFILE_AUTOZONE;
	mutex.lock();
	if (!buffers.size())
	{
		mutex.unlock();
		return false;
	}
	std::shared_ptr<std::vector<uint8_t>> b = buffers[0];
	bufferPtr = b->data();
	bufferSizeInBytes = b->size();
	if (!bufferSizeInBytes)
	{
		mutex.unlock();
		return false;
	}
	return true;
}

void ClientServerMessageStack::unmapOutputBuffer()
{
	buffers.erase(buffers.begin());
	mutex.unlock();
}

GenericEncoder::GenericEncoder()
	: PipelineNode(new GenericEncoder::Private(this))
{
	setNumSlots(1, 1);
}

GenericEncoder::~GenericEncoder()
{
	deconfigure();
}

Result GenericEncoder::configure(GenericEncoderBackendInterface *backend,const char *n)
{
	if (d().m_configured)
	{
		return Result::Node_AlreadyConfigured;
	}
	name=n;
	d().m_configured = true;
	d().m_backend=backend;
	Result result = Result::OK;
	return result;
}

Result GenericEncoder::deconfigure()
{
	if (!d().m_configured)
	{
		return Result::Node_NotConfigured;
	}

	Result result = Result::OK;
	{
		unlinkInput();
	}
	d().m_configured = false;
	d().m_outputPending = false;
	return result;
}

Result GenericEncoder::process(uint64_t timestamp, uint64_t deltaTime)
{
	IOInterface* outputNode = dynamic_cast<IOInterface*>(getOutput(0));
	if (!outputNode)
	{
		return Result::Node_InvalidOutput;
	}
	return d().process(timestamp, outputNode);
}

Result GenericEncoder::Private::process(uint64_t timestamp, IOInterface* outputNode)
{
	if (!m_configured)
	{
		return Result::Node_NotConfigured;
	}
	// Write any pending output.
	return writeOutput(outputNode);
}


Result GenericEncoder::onInputLink(int slot, PipelineNode* node)
{
	if (!d().m_configured)
	{
		AVSLOG(Error) << "GenericEncoder: PipelineNode needs to be configured before it can accept input";
		return Result::Node_NotConfigured;
	}
	assert(d().m_backend);
	return Result::OK;
}

Result GenericEncoder::onOutputLink(int slot, PipelineNode* node)
{
	if (!dynamic_cast<IOInterface*>(node))
	{
		AVSLOG(Error) << "GenericEncoder: Output node does not implement IO operations";
		return Result::Node_Incompatible;
	}
	return Result::OK;
}

void GenericEncoder::onInputUnlink(int slot, PipelineNode* node)
{
	if (!d().m_configured)
	{
		return;
	}
}

Result GenericEncoder::Private::writeOutput(IOInterface* outputNode)
{
	assert(outputNode);
	assert(m_backend);
	void*  mappedBuffer;
	size_t mappedBufferSize;
	size_t bufferCount=0;
	while(bufferCount<1000&&m_backend->mapOutputBuffer(mappedBuffer, mappedBufferSize))
	{
		size_t numBytesWrittenToOutput;
		Result result = outputNode->write(q_ptr(), mappedBuffer, mappedBufferSize, numBytesWrittenToOutput);

		m_backend->unmapOutputBuffer();

		if (!result)
		{
			return result;
		}
		if (numBytesWrittenToOutput < mappedBufferSize)
		{
			AVSLOG(Warning) << "GenericEncoder: Incomplete data written to output node";
			return Result::Failed;
		}
		bufferCount++;

	}
	return Result::OK;
}
