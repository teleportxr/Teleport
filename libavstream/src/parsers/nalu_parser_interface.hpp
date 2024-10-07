// libavstream
// (c) Copyright 2018-2024 Teleport XR Ltd

#pragma once

#include <cstdint>
#include <vector>

#include <libavstream/common.hpp>

namespace avs
{
	class NALUParserInterface
	{
	public:
		virtual ~NALUParserInterface() = default;

		virtual VideoPayloadType classify(const uint8_t* data, size_t size) const = 0;
		virtual bool isFirstVCL(const uint8_t* data, size_t size) const = 0;
		virtual bool isIDR(const uint8_t* data, size_t size) const = 0;

		static constexpr size_t HeaderSize = 2;
	};

} // avs