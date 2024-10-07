// libavstream
// (c) Copyright 2018-2024 Teleport XR Ltd

#pragma once

#include <cassert>
#include <parsers/nalu_parser_interface.hpp>

namespace avs
{
	class NALUParser_H265 final : public NALUParserInterface
	{
	public:
		VideoPayloadType classify(const uint8_t* data, size_t size) const override
		{
			assert(size > 0);
			const uint8_t type = (data[0] & 0x7e) >> 1;
			if (type < 32)
			{
				return VideoPayloadType::VCL;
			}
			else
			{
				switch (type)
				{
				case 32: return VideoPayloadType::VPS;
				case 33: return VideoPayloadType::SPS;
				case 34: return VideoPayloadType::PPS;
				case 39: return VideoPayloadType::ALE;
				}
			}
			return VideoPayloadType::OtherNALUnit;
		}

		bool isFirstVCL(const uint8_t* data, size_t size) const override
		{
			assert(size >= 2);
			return (data[2] & 0x80) != 0;
		}

		bool isIDR(const uint8_t* data, size_t size) const override
		{
			assert(size > 0);
			const uint8_t type = (data[0] & 0x7e) >> 1;
			// IDR_W_DLP or IDR_N_LP  
			if (type == 19 || type == 20)
			{
				return true;
			}
			return false;
		}
	};

} // avs