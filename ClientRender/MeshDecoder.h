#pragma once

namespace teleport
{
	namespace core
	{
		struct DecodedGeometry;
	}
	namespace clientrender
	{
		struct GeometryDecodeData;
		bool DecodeScene(const GeometryDecodeData &geometryDecodeData,core::DecodedGeometry &dg, bool stationary);
	}
}
