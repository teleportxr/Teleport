// libavstream
// (c) Copyright 2018-2024 Teleport XR Ltd

#include "buffer_p.hpp"
#include <algorithm>

namespace avs {

	Buffer::Buffer()
		: PipelineNode(new Buffer::Private(this))
	{
		setNumSlots(1, 1);
	}

	Result Buffer::configure(size_t bufferCapacity)
	{
		std::lock_guard<std::mutex> lock(d().m_mutex);

		try {
			d().m_buffer.resize(bufferCapacity);
		}
		catch (const std::bad_alloc&)
		{
			return Result::IO_OutOfMemory;
		}
		return Result::OK;
	}

	Result Buffer::deconfigure()
	{
		std::lock_guard<std::mutex> lock(d().m_mutex);
		d().m_buffer.resize(0);
		d().m_buffer.shrink_to_fit();
		return Result::OK;
	}

	Result Buffer::read(PipelineNode*, void* buffer, size_t& bufferSize, size_t& bytesRead)
	{
		std::lock_guard<std::mutex> lock(d().m_mutex);

		const size_t bufferCapacity = d().m_buffer.size();
		if (bufferCapacity == 0)
		{
			return Result::Node_NotConfigured;
		}
		if (bufferSize == 0)
		{
			return Result::IO_Retry;
		}

		bytesRead = std::min(bufferSize, d().m_bytesAvailable);
		if (bytesRead > 0)
		{
			if (d().m_readCursor < d().m_writeCursor)
			{
				memcpy(buffer, &d().m_buffer[d().m_readCursor], bytesRead);
			}
			else {
				const size_t N1 = bufferCapacity - d().m_readCursor;
				const size_t N2 = bytesRead - N1;
				memcpy(buffer, &d().m_buffer[d().m_readCursor], N1);
				memcpy(static_cast<char*>(buffer) + N1, &d().m_buffer[0], N2);
			}
			d().m_readCursor = (d().m_readCursor + bytesRead) % bufferCapacity;
			d().m_bytesAvailable -= bytesRead;
		}
		return Result::OK;
	}

	Result Buffer::write(PipelineNode*, const void* buffer, size_t bufferSize, size_t& bytesWritten)
	{
		std::lock_guard<std::mutex> lock(d().m_mutex);

		const size_t bufferCapacity = d().m_buffer.size();
		if (bufferCapacity == 0)
		{
			return Result::Node_NotConfigured;
		}

		bytesWritten = std::min(bufferSize, bufferCapacity - d().m_bytesAvailable);
		if (bytesWritten > 0)
		{
			if (d().m_readCursor < d().m_writeCursor)
			{
				const size_t N1 = bufferCapacity - d().m_writeCursor;
				const size_t N2 = bytesWritten - N1;
				memcpy(&d().m_buffer[d().m_writeCursor], buffer, N1);
				memcpy(&d().m_buffer[0], static_cast<const char*>(buffer) + N1, N2);
			}
			else {
				memcpy(&d().m_buffer[d().m_writeCursor], buffer, bytesWritten);
			}
			d().m_writeCursor = (d().m_writeCursor + bytesWritten) % bufferCapacity;
			d().m_bytesAvailable += bytesWritten;
		}
		return Result::OK;
	}

	size_t Buffer::getCapacity() const
	{
		std::lock_guard<std::mutex> lock(d().m_mutex);
		return d().m_buffer.size();
	}

} // avs