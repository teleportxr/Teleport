// libavstream
// (c) Copyright 2018-2022 Simul Software Ltd

#include <algorithm>
#include <iostream>
#include <libavstream/singlequeue.h>

#define SAFE_DELETE_ARRAY(p)		{ if(p) { delete[] p; (p)=nullptr; } }

namespace avs
{
	struct SingleQueue::Private final : public PipelineNode::Private
	{
		AVSTREAM_PRIVATEINTERFACE(SingleQueue, PipelineNode)
	};
} // avs
using namespace avs;

SingleQueue::SingleQueue()
	: PipelineNode(new SingleQueue::Private(this))
{
	setNumSlots(1, 1);
	data = (SingleQueue::Private*)this->m_d;
}

SingleQueue::~SingleQueue()
{
	flushInternal();
}

Result SingleQueue::configure(size_t maxBufferSize,  const char *n)
{
	if (maxBufferSize == 0 )
	{
		return Result::Node_InvalidConfiguration;
	}
	name=n;
	std::lock_guard<std::mutex> lock(m_mutex);
	flushInternal();
	buffer.resize(maxBufferSize);
	dataSize = 0;
	return Result::OK;
}

Result SingleQueue::deconfigure()
{
	std::lock_guard<std::mutex> lock(m_mutex);
	flushInternal();
	return Result::OK;
}

void SingleQueue::flush()
{
	std::lock_guard<std::mutex> lock(m_mutex);
	flushInternal();
}

Result SingleQueue::read(PipelineNode*, void* target, size_t& bufferSize, size_t& bytesRead)
{
	bytesRead = 0;
	std::lock_guard<std::mutex> lock(m_mutex);
	if (dataSize == 0)
	{
		return Result::IO_Empty;
	}

	if (!buffer.size() || bufferSize < dataSize)
	{
		bufferSize = dataSize;
		return Result::IO_Retry;
	}

	memcpy(target, buffer.data(), dataSize);
	bytesRead = dataSize;
	pop();

	return Result::OK;
}

Result SingleQueue::write(PipelineNode*, const void* source, size_t bufferSize, size_t& bytesWritten)
{
	if (!buffer.size())
	{
		AVSLOG("SingleQueue::write error: no buffers, unconfigured?\n");
		return Result::Failed;
	}
	std::lock_guard<std::mutex> lock(m_mutex);
	if (bufferSize > buffer.size())
	{
		increaseBufferSize(bufferSize);
#if LIBAVSTREAM_DEBUG_MESSAGES
			std::cerr << name.c_str() << " SingleQueue::write: Buffer size is "<<bufferSize<<" exceeding max. Increasing max to "<<m_maxBufferSize<<" Have "<<m_numElements<<" buffers.\n";
#endif
	}
	push(source, bufferSize);
	bytesWritten = bufferSize;
	return Result::OK;
}

void SingleQueue::flushInternal()
{
	buffer.clear();
}

void SingleQueue::increaseBufferSize(size_t requestedSize)
{
	const size_t oldBufferSize = dataSize;
	std::vector<uint8_t> new_buffer(requestedSize + (requestedSize / 2));
	if (dataSize > 0)
	{
		memcpy(new_buffer.data(),buffer.data(), oldBufferSize);
	}
	buffer = std::move(new_buffer);
}

void SingleQueue::push(const void* data, size_t bytes)
{
	if (!buffer.size())
	{
		//AVSLOG("SingleQueue error: no buffers, unconfigured?\n");
		return;
	}
	if (bytes>buffer.size())
	{
		increaseBufferSize(bytes);
	}
	memcpy(buffer.data(), data, bytes);
#ifdef DEBUG_CLIENTMESSAGES
#pragma pack(push, 1)
	struct ClientMessage
	{
		/// Specifies what type of client message this is.
		uint8_t clientMessagePayloadType;
		uint64_t timestamp_unix_ms;
	};
#pragma pack(pop)
	ClientMessage* cm = (ClientMessage*)buffer.data();
	if (cm->clientMessagePayloadType > 10)
	{
		std::cerr << " bad [payload\n ";
	}
#endif
	dataSize= bytes;
}

void SingleQueue::pop()
{
	dataSize = 0;
}

void SingleQueue::drop()
{
	dataSize = 0;
}

