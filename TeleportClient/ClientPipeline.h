#pragma once
#include <libavstream/common.hpp>
#include <libavstream/pipeline.hpp>
#include <libavstream/network/networksource.h>
#include <libavstream/surface.hpp>
#include <libavstream/queue.hpp>
#include <libavstream/lock_free_queue.h>
#include <libavstream/singlequeue.h>
#include <libavstream/decoder.hpp>
#include <libavstream/geometrydecoder.hpp>
#include <libavstream/mesh.hpp>
#include <libavstream/tagdatadecoder.hpp>
#include <libavstream/audiodecoder_opus.h>
#if !defined(PLATFORM_ANDROID)
#include <libavstream/audioencoder_opus.h>
#endif
#include <libavstream/genericdecoder.h>
#include <libavstream/audio/audiotarget.h>
#include "TeleportCore/CommonNetworking.h"

namespace teleport
{
	namespace client
	{
		//! Contains the full pipeline and member nodes for the client.
		class ClientPipeline
		{
		public:
			ClientPipeline();
			~ClientPipeline();
			bool Init(const teleport::core::SetupCommand& setupCommand, const char* server_ip);
			void Shutdown();
		
			//! Break the connection without logging off - the server should initiate reconnection.
			void Debug_BreakConnection();

			// Pipeline and nodes:
			avs::Pipeline pipeline;

			avs::LockFreeQueue unreliableToServerQueue;
			avs::LockFreeQueue reliableToServerQueue;
			avs::SingleQueue nodePosesQueue;
			avs::SingleQueue inputStateQueue;
			std::shared_ptr<avs::NetworkSource> source;
			avs::Queue videoQueue;
			avs::Decoder decoder;
			avs::Surface surface;

			avs::Queue tagDataQueue;
			avs::TagDataDecoder tagDataDecoder;

			avs::Queue geometryQueue;
			avs::GeometryDecoder avsGeometryDecoder;
			avs::GeometryTarget avsGeometryTarget;

			// Inbound WebRTC Opus voices are decoded and spatialised by
			// teleport::audio::SpatialAudioMixer (owned by InstanceRenderer), keyed
			// by the emitting node uid; they no longer flow through a libavstream
			// decoder/target here.

			// Outbound mic path. PCM captured from the audio player
			// is fed into opusAudioEncoder; encoded Opus packets are forwarded
			// to WebRtcNetworkSource::sendOpusFrame via the encoder's frame
			// callback. The encoder runs as a pipeline node so it is ticked
			// each frame to drain any partially-buffered PCM.
#if !defined(PLATFORM_ANDROID)
			avs::OpusAudioEncoder opusAudioEncoder;
#endif

			avs::LockFreeQueue reliableFromServerQueue;
			avs::GenericDecoder commandDecoder;

			avs::DecoderParams decoderParams = {};
		};
	}
}