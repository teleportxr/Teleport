#pragma once
#include "Platform/CrossPlatform/AxesStandard.h"
namespace teleport
{
	namespace core
	{
		struct DecodedGeometry;
	}
	namespace clientrender
	{
		struct GeometryDecodeData;
		bool DecodeScene(const GeometryDecodeData			  &geometryDecodeData,
						 core::DecodedGeometry				  &dg,
						 bool								   stationary,
						 platform::crossplatform::AxesStandard sourceAxesStand);
	}
}
