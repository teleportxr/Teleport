#pragma once

#include <memory>

#include "libavstream/common.hpp"
#include "libavstream/queue.hpp"

#include "NetworkPipeline.h"

namespace SCServer
{
	struct CasterContext
	{
		std::unique_ptr<NetworkPipeline> NetworkPipeline;
		std::unique_ptr<avs::Queue> ColorQueue;
		std::unique_ptr<avs::Queue> DepthQueue;
		std::unique_ptr<avs::Queue> GeometryQueue;
		std::unique_ptr<avs::Queue> AudioQueue;
		bool isCapturingDepth = false;
		avs::AxesStandard axesStandard = avs::AxesStandard::NotInitialized;
	};
}