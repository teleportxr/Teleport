#pragma once
#include <libavstream/common.hpp>
#include <libavstream/pipeline.hpp>
#include <libavstream/networksource.hpp>
#include <libavstream/surface.hpp>
#include <libavstream/queue.hpp>
#include <libavstream/decoder.hpp>
#include <libavstream/geometrydecoder.hpp>
#include <libavstream/mesh.hpp>

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
			avs::Pipeline pipeline;

			avs::NetworkSource source;
			avs::Queue videoQueue;
			avs::Decoder decoder;
			avs::Surface surface;

			avs::Queue geometryQueue;
			avs::GeometryDecoder avsGeometryDecoder;
			avs::GeometryTarget avsGeometryTarget;
		};
	}
}