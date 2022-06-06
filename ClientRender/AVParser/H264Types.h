// (C) Copyright 2018-2022 Simul Software Ltd

#pragma once

#include <memory>
#include <vector>
#include <array>
#include <cstdint>
#include <cstddef>

namespace avparser
{
	namespace h264
	{
		enum class NALUnitType
		{
			UNSPECIFIED = 0,
			SLICE = 1,
			DPA = 2,
			DPB = 3,
			DPC = 4,
			IDR_SLICE = 5,
			SEI = 6,
			SPS = 7,
			PPS = 8,
			AUD = 9,
			END_SEQUENCE = 10,
			END_STREAM = 11,
			FILLER_DATA = 12,
			SPS_EXT = 13,
			PREFIX = 14,
			SUB_SPS = 15,
			DPS = 16,
			RESERVED17 = 17,
			RESERVED18 = 18,
			AUXILIARY_SLICE = 19,
			EXTEN_SLICE = 20,
			DEPTH_EXTEN_SLICE = 21,
			RESERVED22 = 22,
			RESERVED23 = 23,
			UNSPECIFIED24 = 24,
			UNSPECIFIED25 = 25,
			UNSPECIFIED26 = 26,
			UNSPECIFIED27 = 27,
			UNSPECIFIED28 = 28,
			UNSPECIFIED29 = 29,
			UNSPECIFIED30 = 30,
			UNSPECIFIED31 = 31
		};
	}
}

