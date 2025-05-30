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
#include <libavstream/audiodecoder.h>
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

			avs::Queue audioQueue;
			avs::AudioDecoder avsAudioDecoder;
			avs::AudioTarget avsAudioTarget;

			avs::LockFreeQueue reliableOutQueue;
			avs::GenericDecoder commandDecoder;

			avs::DecoderParams decoderParams = {};
		};
	}
}