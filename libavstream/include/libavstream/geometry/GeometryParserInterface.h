// libavstream
// (c) Copyright 2018-2022 Simul Software Ltd

#pragma once
#include <vector>
#include <map>
#include <memory>
#include <set>

#include <libavstream/common.hpp>

namespace avs
{

	class GeometryParserInterface
	{
	public:
		virtual ~GeometryParserInterface() = default;
		virtual GeometryPayloadType classify(const uint8_t* buffer, size_t bufferSize, size_t& dataOffset) const = 0;
		static constexpr size_t HeaderSize = 2;
	};
}