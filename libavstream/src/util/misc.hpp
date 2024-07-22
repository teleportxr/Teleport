// libavstream
// (c) Copyright 2018-2024 Simul Software Ltd

#pragma once

#include <cstdint>

namespace avs
{

template<class T, class TimestampType=uint32_t>
struct ValueWithTimestamp
{
	T value;
	TimestampType timestamp;

	operator T() const { return value; }
	T* operator->() { return &value; }
	const T* operator->() const { return &value; }
};

} // avs