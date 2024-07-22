// libavstream
// (c) Copyright 2018-2024 Simul Software Ltd

#pragma once

#include <cassert>
#include <cstdint>

#include <util/bytebuffer.hpp>

namespace avs
{

	class BinaryReader
	{
	public:
		BinaryReader(const ByteBuffer& buffer,size_t offset=0) : m_offset(offset), m_buffer(buffer)
		{}

		template<typename T> T get()
		{
			assert(m_offset + sizeof(T) < m_buffer.size());
			const T value = *reinterpret_cast<const T*>(&m_buffer[m_offset]);
			m_offset += sizeof(T);
			return value;
		}

		size_t offset() const
		{
			return m_offset;
		}

	private:
		size_t m_offset = 0;
		const ByteBuffer& m_buffer;
	};

	class BinaryWriter
	{
	public:
		BinaryWriter(ByteBuffer& buffer) : m_buffer(buffer)
		{}

		template<typename T> void put(T value)
		{
			const size_t offset = m_buffer.size();
			m_buffer.resize(offset + sizeof(T));
			*reinterpret_cast<T*>(&m_buffer[offset]) = value;
		}

		size_t offset() const
		{
			return m_buffer.size();
		}

	private:
		ByteBuffer& m_buffer;
	};

} // avs
