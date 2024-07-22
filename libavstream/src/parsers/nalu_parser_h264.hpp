// libavstream
// (c) Copyright 2018-2024 Simul Software Ltd

#pragma once

#include <cassert>
#include <parsers/nalu_parser_interface.hpp>

namespace avs
{
	class NALUParser_H264 final : public NALUParserInterface
	{
	public:
		// Aidan: Refer to https://yumichan.net/video-processing/video-compression/introduction-to-h264-nal-unit/
		VideoPayloadType classify(const uint8_t* data, size_t size) const override
		{
			assert(size > 0);
			// Aidan: NAL-Unit type contained in the first 5 bits of first byte
			// A value of 1 is a coded slice of a non-IDR frame and 5 is a coded slice of an IDR frame
			const uint8_t type = data[0] & 31;
			if (type == 1 || type == 5)
			{
				return VideoPayloadType::VCL;
			}
			else
			{
				switch (type)
				{
				case 7: return VideoPayloadType::SPS;
				case 8: return VideoPayloadType::PPS;
				}
			}

			return VideoPayloadType::OtherNALUnit;
		}

		// Not needed anymore
		bool isFirstVCL(const uint8_t* data, size_t size) const override
		{
			return true;
		}

		bool isIDR(const uint8_t* data, size_t size) const override
		{
			assert(size > 0);
			if ((data[0] & 31) == 5)
			{
				return true;
			}
			return false;
		}
	};

} // avs
