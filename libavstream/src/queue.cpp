// libavstream
// (c) Copyright 2018-2022 Simul Software Ltd

#include "queue_p.hpp"
#include <algorithm>
#include <iostream>

#define SAFE_DELETE_ARRAY(p)		{ if(p) { delete[] p; (p)=nullptr; } }

namespace avs
{
	Queue::Queue()
		: PipelineNode(new Queue::Private(this))
	{
		setNumSlots(1, 1);
		data = (Queue::Private*)this->m_d;
	}

	Queue::~Queue()
	{
		flushInternal();
	}

	Result Queue::configure(size_t maxBufferSize, size_t maxBuffers, const char *n)
	{
		if (maxBufferSize == 0 || maxBuffers == 0)
		{
			return Result::Node_InvalidConfiguration;
		}
		name=n;
		std::lock_guard<std::mutex> lock(m_mutex);
		flushInternal();
		m_originalMaxBufferSize = maxBufferSize;
		m_originalMaxBuffers = maxBuffers;
		m_maxBufferSize = maxBufferSize;
		m_maxBuffers = maxBuffers;
		m_numElements = 0;
		m_front = -1;
		m_mem = new char[m_maxBuffers * m_maxBufferSize];
		m_dataSizes = new size_t[m_maxBuffers];
		for (size_t i = 0; i < m_maxBuffers; ++i)
		{
			m_dataSizes[i] = 0;
		}
		return Result::OK;
	}

	Result Queue::deconfigure()
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		flushInternal();
		m_originalMaxBufferSize = 0;
		m_originalMaxBuffers = 0;
		m_maxBufferSize = 0;
		m_maxBuffers = 0;
		m_numElements = 0;
		m_front = -1;
		return Result::OK;
	}

	void Queue::flush()
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		flushInternal();
	}

	Result Queue::read(PipelineNode*, void* buffer, size_t& bufferSize, size_t& bytesRead)
	{
		bytesRead = 0;
		std::lock_guard<std::mutex> lock(m_mutex);
		if (m_numElements == 0)
		{
			return Result::IO_Empty;
		}

		size_t frontSize;
		const void* front = frontp(frontSize);
		if (!buffer || bufferSize < frontSize)
		{
			bufferSize = frontSize;
			return Result::IO_Retry;
		}

		memcpy(buffer, front, frontSize);
		bytesRead = frontSize;
		pop();

		return Result::OK;
	}

	Result Queue::write(PipelineNode*, const void* buffer, size_t bufferSize, size_t& bytesWritten)
	{
		if (!m_maxBuffers)
		{
			AVSLOG("Queue::write error: no buffers, unconfigured?\n");
			return Result::Failed;
		}
		std::lock_guard<std::mutex> lock(m_mutex);
		if (m_numElements == m_maxBuffers)
		{
			auto oldsize=m_maxBuffers;
			if(m_maxBuffers< m_absoluteMaxBuffers)
				increaseBufferCount();
			else
			{
				AVSLOG_NOSPAM(Warning) << name.c_str() << " Queue::write: Max buffers " << m_maxBuffers << ", can't increase any more.\n";
				return Result::IO_Full;
			}
#if LIBAVSTREAM_DEBUG_MESSAGES
			AVSLOG(Warning) << name.c_str() << " Queue::write: Max buffers " << oldsize << " reached. Increasing max to " << m_maxBuffers << ".\n";
#endif
		}
		if (bufferSize > m_maxBufferSize)
		{
			increaseBufferSize(bufferSize);
#if LIBAVSTREAM_DEBUG_MESSAGES
			AVSLOG(Warning) << name.c_str() << " Queue::write: Buffer size is " << bufferSize << " exceeding max. Increasing max to " << m_maxBufferSize << " Have " << m_numElements << " buffers.\n";
#endif
		}
		
		push(buffer, bufferSize);

		bytesWritten = bufferSize;

		return Result::OK;
	}

	void Queue::flushInternal()
	{
		SAFE_DELETE_ARRAY(m_mem)
		SAFE_DELETE_ARRAY(m_dataSizes)
	}

	void Queue::increaseBufferCount()
	{
		const size_t oldBufferCount = m_maxBuffers;
		
		m_maxBuffers += std::max(m_maxBuffers,m_originalMaxBuffers)/2;
		char* oldMem = m_mem;
		m_mem = new char[m_maxBuffers * m_maxBufferSize];

		size_t* oldSizes = m_dataSizes;
		m_dataSizes = new size_t[m_maxBuffers];

		// Ensure any used sections of current memory go to the beginning
		if (m_numElements > 0)
		{
			const size_t frontToEnd = std::min<size_t>(oldBufferCount - m_front, m_numElements);
			const size_t firstCopySize = frontToEnd * m_maxBufferSize;

			memcpy(m_mem, &oldMem[m_front * m_maxBufferSize], firstCopySize);
			memcpy(m_dataSizes, &oldSizes[m_front], sizeof(size_t) * frontToEnd);

			// Account for potential wrap around items stored before front in memory
			const int64_t startToFront = m_numElements - frontToEnd;
			if (startToFront > 0)
			{
				memcpy(&m_mem[firstCopySize], &oldMem[(m_front - startToFront) * m_maxBufferSize], startToFront * m_maxBufferSize);
				memcpy(&m_dataSizes[frontToEnd], &oldSizes[m_front - startToFront], startToFront * sizeof(size_t));
			}
			m_front = 0;
		}
		else
		{
			m_front = -1;
		}

		delete[] oldMem;
		delete[] oldSizes;
	}

	void Queue::increaseBufferSize(size_t requestedSize)
	{
		const size_t oldBufferSize = m_maxBufferSize;
		m_maxBufferSize = requestedSize + (requestedSize / 2);
		char* oldMem = m_mem;
		m_mem = new char[m_maxBuffers * m_maxBufferSize];

		if (m_numElements > 0)
		{
			const int64_t frontToEnd = std::min<size_t>(m_maxBuffers - m_front, m_numElements);
			const int64_t startToFront = m_numElements - frontToEnd;

			// Start to front if applicable
			for (int64_t i = m_front - startToFront; i < m_front; ++i)
			{
				memcpy(&m_mem[i * m_maxBufferSize], &oldMem[i * oldBufferSize], oldBufferSize);
			}
		
			// Front to end
			for (int64_t i = m_front; i < m_front + frontToEnd; ++i)
			{
				memcpy(&m_mem[i * m_maxBufferSize], &oldMem[i * oldBufferSize], oldBufferSize);
			}
		}

		delete[] oldMem;
	}

	const void* Queue::frontp(size_t& bufferSize) const
	{
		bufferSize = m_dataSizes[m_front];
		return &m_mem[m_front * m_maxBufferSize];
	}

	void Queue::push(const void* buffer, size_t bufferSize)
	{
		if (m_numElements == 0)
		{
			m_front = 0;
		}
		if (!m_maxBuffers)
		{
			AVSLOG("Queue error: no buffers, unconfigured?\n");
			return;
		}
		const int64_t index = (m_front + m_numElements) % m_maxBuffers;
		memcpy(&m_mem[index * m_maxBufferSize], buffer, bufferSize);
		m_dataSizes[index] = bufferSize;
		m_numElements++;
	}

	void Queue::pop()
	{
		m_dataSizes[m_front] = 0;
		m_numElements--;
		if (m_numElements == 0)
		{
			m_front = -1;
		}
		else
		{
			m_front = (m_front + 1) % m_maxBuffers;
		}	
	}

} // avs