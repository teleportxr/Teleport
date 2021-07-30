// libavstream
// (c) Copyright 2018-2019 Simul Software Ltd

#include <libavstream/timer.hpp>

namespace avs 
{
	Timer::Timer()
	{
		memset(&(m_startTimestamp), 0, sizeof(Timestamp));
		Start();
	}
	
	void Timer::Start()
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		m_startTimestamp = Platform::getTimestamp();
	}

	double Timer::GetElapsedTime()
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		// Convert from nanoseconds to seconds.
		return 0.000001 * Platform::getTimeElapsed(m_startTimestamp, Platform::getTimestamp());
	}

	Timer TimerUtil::m_timer;

	void TimerUtil::Start()
	{
		m_timer.Start();
	}

	double TimerUtil::GetElapsedTime()
	{
		return m_timer.GetElapsedTime();
	}
} 