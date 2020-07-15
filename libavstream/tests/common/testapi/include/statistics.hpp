// libavstream
// (c) Copyright 2018 Simul.co

#pragma once

#include <vector>

namespace avs::test {

template<class T>
class StatBuffer
{
public:
	StatBuffer(size_t capacity)
		: m_capacity(capacity)
	{
		m_buffer.reserve(capacity);
	}

	void push(T value)
	{
		if(m_buffer.size() == m_capacity) {
			m_buffer.erase(std::begin(m_buffer));
		}
		m_buffer.push_back(value);
	}

	int count() const
	{
		return (int)m_buffer.size();
	}

	const T* data() const
	{
		return m_buffer.data();
	}

	void reset()
	{
		m_buffer.clear();
	}

private:
	std::vector<T> m_buffer;
	size_t m_capacity;
};

} // avs::test