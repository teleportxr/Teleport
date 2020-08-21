// libavstream
// (c) Copyright 2018-2019 Simul Software Ltd

#include <queue_p.hpp>

namespace avs
{
	Queue::Queue()
		: Node(new Queue::Private(this))
	{
		setNumSlots(1, 1);
		data = (Queue::Private*)this->m_d;
	}

	Result Queue::configure(size_t maxBuffers,const char *n)
	{
		if (maxBuffers == 0)
		{
			return Result::Node_InvalidConfiguration;
		}
		data->name=n;
		std::lock_guard<std::mutex> lock(data->m_mutex);
		data->flushInternal();
		data->m_maxBuffers = maxBuffers;
		return Result::OK;
	}

	Result Queue::deconfigure()
	{
		std::lock_guard<std::mutex> lock(data->m_mutex);
		data->flushInternal();
		data->m_maxBuffers = 0;
		return Result::OK;
	}

	void Queue::flush()
	{
		std::lock_guard<std::mutex> lock(data->m_mutex);
		data->flushInternal();
	}

	Result Queue::read(Node*, void* buffer, size_t& bufferSize, size_t& bytesRead)
	{
		bytesRead = 0;
		std::lock_guard<std::mutex> lock(data->m_mutex);
		if (data->m_buffers.empty())
		{
			return Result::IO_Empty;
		}

		const auto& front = data->m_buffers.front();
		if (!buffer || bufferSize < front.size())
		{
			bufferSize = front.size();
			return Result::IO_Retry;
		}

		std::memcpy(buffer, front.data(), front.size());
		bytesRead = front.size();
		data->m_buffers.pop();
		return Result::OK;
	}

	Result Queue::write(Node*, const void* buffer, size_t bufferSize, size_t& bytesWritten)
	{
		std::lock_guard<std::mutex> lock(data->m_mutex);
		if (data->m_buffers.size() == data->m_maxBuffers)
		{
			AVSLOG(Warning) << data->name.c_str()<<" Queue::write: out of buffers.\n";
			return Result::IO_Full;
		}
		try
		{
			data->m_buffers.emplace(static_cast<const char*>(buffer), static_cast<const char*>(buffer) + bufferSize);
		}
		catch (const std::bad_alloc&)
		{
			return Result::IO_OutOfMemory;
		}
		bytesWritten = bufferSize;
		return Result::OK;
	}

	Result Queue::amend(Node*, const void* buffer, size_t bufferSize, size_t& bytesWritten)
	{
		std::lock_guard<std::mutex> lock(data->m_mutex);
		if (data->m_buffers.size() == data->m_maxBuffers)
		{
			AVSLOG(Warning) << data->name.c_str() << " Queue::amend: out of buffers.";
			return Result::IO_Full;
		}
		try
		{

			// Adds the data in the new buffer to the last buffer in the queue
			if (data->m_buffers.empty())
			{
				return Result::IO_Empty;
			}

			std::vector<char>& lastBuffer = data->m_buffers.back();
			size_t currentSize = lastBuffer.size();
			lastBuffer.resize(currentSize + bufferSize);
			memcpy(lastBuffer.data() + currentSize, static_cast<const char*>(buffer), bufferSize);
		}
		catch (const std::bad_alloc&)
		{
			return Result::IO_OutOfMemory;
		}
		bytesWritten = bufferSize;
		return Result::OK;
	}

	void Queue::Private::flushInternal()
	{
		while (!m_buffers.empty())
		{
			m_buffers.pop();
		}
	}

} // avs