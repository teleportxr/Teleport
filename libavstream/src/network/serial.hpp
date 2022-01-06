// libavstream
// (c) Copyright 2018-2022 Simul Software Ltd

#pragma once

#include <limits>
#include <type_traits>

namespace avs
{
	template<typename T>
	class Serial
	{
		static_assert(std::numeric_limits<T>::is_integer, "Serial numbers must be integer numeric types");
		static_assert(!std::numeric_limits<T>::is_signed, "Serial numbers must be of unsigned type");
	public:
		Serial(T initialValue = T(0)) : m_value(initialValue)
		{}

		operator T() const
		{
			return m_value;
		}
		Serial<T>& operator=(T value)
		{
			m_value = value;
			return *this;
		}

		bool operator==(const Serial<T>& other) const
		{
			return m_value == other.m_value;
		}
		bool operator!=(const Serial<T>& other) const
		{
			return m_value != other.m_value;
		}
		bool operator<(const Serial<T>& other) const
		{
			return distance(*this, other) < 0;
		}

		Serial<T> next() const
		{
			return Serial<T>(m_value + 1);
		}
		Serial<T> prev() const
		{
			return Serial<T>(m_value - 1);
		}
		static typename std::make_signed<T>::type distance(const Serial<T>& s1, const Serial<T>& s2)
		{
			return s1.m_value - s2.m_value;
		}

		static constexpr T MaxValue = std::numeric_limits<T>::max();
		static constexpr T MaxDistance = std::numeric_limits<typename std::make_signed<T>::type>::max();

	private:
		T m_value;
	};

} // avs