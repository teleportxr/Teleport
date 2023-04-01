#pragma once

#include <memory>

#include "libavstream/common.hpp"
#include "libavstream/queue.hpp"

#include "NetworkPipeline.h"
#include "SourceNetworkPipeline.h"
#include <libavstream/audiodecoder.h>
#include <libavstream/audio/audiotarget.h>
#include <CustomAudioStreamTarget.h>

namespace teleport
{
	namespace server
	{
		//! Wrapper for the network pipeline objects for a given client.
		//! //
		struct ClientNetworkContext
		{
			ClientNetworkContext();
			void Init(avs::uid clientID,bool receive_audio);
			// Sending
			NetworkPipeline NetworkPipeline;

			// Receiving
			SourceNetworkPipeline sourceNetworkPipeline;
			avs::Queue sourceAudioQueue;
			avs::AudioDecoder audioDecoder;
			avs::AudioTarget audioTarget;
			audio::CustomAudioStreamTarget audioStreamTarget;

			bool isCapturingDepth = false;
			avs::AxesStandard axesStandard = avs::AxesStandard::NotInitialized;
		};
	}
}