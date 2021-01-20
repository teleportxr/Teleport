#pragma once

#include <memory>

#include "libavstream/common.hpp"
#include "libavstream/queue.hpp"

#include "NetworkPipeline.h"
#include "SourceNetworkPipeline.h"
#include <libavstream/audiodecoder.h>
#include <libavstream/audio/audiotarget.h>
#include <crossplatform/CustomAudioStreamTarget.h>

namespace SCServer
{
	struct CasterContext
	{
		// Sending
		std::unique_ptr<NetworkPipeline> NetworkPipeline;
		std::unique_ptr<avs::Queue> ColorQueue;
		std::unique_ptr<avs::Queue> DepthQueue;
		std::unique_ptr<avs::Queue> GeometryQueue;
		std::unique_ptr<avs::Queue> AudioQueue;

		// Receiving
		std::unique_ptr<SourceNetworkPipeline> sourceNetworkPipeline;
		std::unique_ptr<avs::Queue> sourceAudioQueue;
		std::unique_ptr<avs::AudioDecoder> audioDecoder;
		std::unique_ptr<avs::AudioTarget> audioTarget;
		std::unique_ptr<sca::CustomAudioStreamTarget> audioStreamTarget;

		bool isCapturingDepth = false;
		avs::AxesStandard axesStandard = avs::AxesStandard::NotInitialized;
	};
}