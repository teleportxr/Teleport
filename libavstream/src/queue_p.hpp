// libavstream
// (c) Copyright 2018-2019 Simul Software Ltd

#pragma once

#include <mutex>

#include <common_p.hpp>
#include <node_p.hpp>
#include <libavstream/queue.hpp>

namespace avs
{

	struct Queue::Private final : public Node::Private
	{
		std::string name;
		AVSTREAM_PRIVATEINTERFACE(Queue, Node)
		/** Contiguous memory that contains buffers of equal size */
		char* m_mem = nullptr;
		/** Contains sizes of data in each buffer */
		size_t* m_dataSizes = nullptr;
		size_t m_originalMaxBufferSize = 0;
		size_t m_originalMaxBuffers = 0;
		size_t m_maxBufferSize = 0;
		size_t m_maxBuffers = 0;
		size_t m_numElements = 0;
		int64_t m_front = -1;
		std::mutex m_mutex;
		void flushInternal();
		void increaseBufferCount();
		void increaseBufferSize(size_t requestedSize);
		const void* front(size_t& bufferSize) const;
		void push(const void* buffer, size_t bufferSize);
		void pop();
	};

} // avs