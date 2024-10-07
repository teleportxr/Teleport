// libavstream
// (c) Copyright 2018-2024 Teleport XR Ltd

#pragma once

#include <cstdint>
#include <vector>
#include <optional>

namespace avs
{

	using ByteBuffer = std::vector<uint8_t>;
	using OptionalByteBuffer = std::optional<ByteBuffer>;

	class BufferPool
	{
	public:
		using Handle = size_t;

		Handle acquire(size_t capacity = 0)
		{
			std::optional<Handle> handle;
			for (Handle h = 0; h < m_buffers.size(); ++h)
			{
				if (!m_buffers[h].has_value())
				{
					handle = h;
					break;
				}
			}
			if (!handle.has_value())
			{
				handle.emplace(m_buffers.size());
				m_buffers.emplace_back();
			}

			auto& buffer = m_buffers[handle.value()];
			if (!buffer.has_value())
			{
				buffer.emplace();
				buffer->reserve(capacity);
			}
			return handle.value();
		}

		Handle acquire(ByteBuffer&& data)
		{
			Handle handle = acquire();
			buffer(handle) = data;
			return handle;
		}

		void release(Handle handle)
		{
			assert(handle < m_buffers.size());
			m_buffers[handle].reset();
		}

		ByteBuffer& buffer(Handle handle)
		{
			assert(handle < m_buffers.size());
			assert(m_buffers[handle].has_value());
			return m_buffers[handle].value();
		}

		void clear()
		{
			m_buffers.clear();
			m_buffers.shrink_to_fit();
		}

		size_t size() const
		{
			return m_buffers.size();
		}

	private:
		std::vector<OptionalByteBuffer> m_buffers;
	};

} // avs