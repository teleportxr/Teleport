// libavstream
// (c) Copyright 2018-2019 Simul Software Ltd

#pragma once

#include <cstdint>

namespace avs
{

struct SystemTime
{
	uint16_t hour;
	uint16_t minute;
	uint16_t second;
	uint16_t ms;
};

} // avs