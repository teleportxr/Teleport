// libavstream
// (c) Copyright 2018-2022 Simul Software Ltd

#pragma once

#include <cassert>
#include <queue>
#include <util/misc.hpp>

namespace avs
{

	template<class T, class TimestampType = uint32_t>
	class JitterBuffer
	{
	public:
		enum class State
		{
			Normal,
			Buffering,
			Bursting,
		};
		JitterBuffer(TimestampType nominalLength, TimestampType maxLength)
			: m_nominalLength(nominalLength)
			, m_maxLength(maxLength)
		{}

		void push(TimestampType timestamp, T&& data)
		{
			if (!m_buffer.empty())
			{
				const TimestampType prevBackTimestamp = m_buffer.back().timestamp;
				const TimestampType dT = timestamp - prevBackTimestamp;
				m_duration += dT;
			}

			m_buffer.push(ValueWithTimestamp<T, TimestampType>{data, timestamp});

			if (m_duration >= m_maxLength)
			{
				m_state = State::Bursting;
			}
			else if (m_duration >= m_nominalLength)
			{
				m_state = State::Normal;
			}
		}

		void pop()
		{
			assert(!m_buffer.empty());
			assert(!isLocked());

			const TimestampType prevFrontTimestamp = m_buffer.front().timestamp;
			m_buffer.pop();

			if (m_buffer.empty())
			{
				m_duration = TimestampType(0);
				m_state = State::Buffering;
			}
			else
			{
				const TimestampType dT = m_buffer.front().timestamp - prevFrontTimestamp;
				m_duration -= dT;

				if (m_state == State::Bursting && m_duration <= m_nominalLength)
				{
					m_state = State::Normal;
				}
				if (m_state != State::Bursting)
				{
					m_readEnable = false;
				}
			}
		}

		void advance()
		{
			m_readEnable = true;
		}

		const T* front() const
		{
			if (isLocked() || m_buffer.empty())
			{
				return nullptr;
			}
			return &m_buffer.front().value;
		}
		T* front()
		{
			if (isLocked() || m_buffer.empty())
			{
				return nullptr;
			}
			return &m_buffer.front().value;
		}

		TimestampType getDuration() const
		{
			return m_duration;
		}

		bool isLocked() const
		{
			if (m_state == State::Normal)
			{
				return !m_readEnable;
			}
			else
			{
				return m_state == State::Buffering;
			}
		}

	private:
		std::queue<ValueWithTimestamp<T, TimestampType>> m_buffer;
		TimestampType m_duration = TimestampType(0);
		State m_state = State::Buffering;
		bool m_readEnable = false;

		TimestampType m_nominalLength;
		TimestampType m_maxLength;
	};

} // avs