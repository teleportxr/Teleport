#pragma once
#include <libavstream/common.hpp>
#include <libavstream/pipeline.hpp>
#include <libavstream/network/networksource.h>
#include <libavstream/surface.hpp>
#include <libavstream/queue.hpp>
#include <libavstream/decoder.hpp>
#include <libavstream/geometrydecoder.hpp>
#include <libavstream/mesh.hpp>
#include <libavstream/tagdatadecoder.hpp>
#include <libavstream/audiodecoder.h>
#include <libavstream/audio/audiotarget.h>

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
			void Shutdown();

			// Pipeline and nodes:
			avs::Pipeline pipeline;

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

			avs::DecoderParams decoderParams = {};
			avs::VideoConfig videoConfig;
		};
	}
}