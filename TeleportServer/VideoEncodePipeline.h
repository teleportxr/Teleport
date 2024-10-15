#pragma once

#include <memory>
#include "ClientNetworkContext.h"
#include "TeleportServer/ServerSettings.h"
#include "TeleportServer/Export.h"

// Forward declare so classes that include don't have to know about them
namespace avs
{
	class Pipeline;
	class Encoder;
	class Surface;
}

namespace teleport
{
	namespace server
	{

		//! Wrapper for the video encoding pipeline objects.
		class TELEPORT_SERVER_API VideoEncodePipeline
		{
		public:
			VideoEncodePipeline() = default;
			virtual ~VideoEncodePipeline();

			Result initialize( const VideoEncodeParams& videoEncodeParams, avs::PipelineNode* videoOutput, avs::PipelineNode* tagDataOutput);

			Result configure( const VideoEncodeParams& videoEncodeParams, avs::PipelineNode* colorQueue, avs::PipelineNode* tagDataQueue);
			Result reconfigure1( const VideoEncodeParams& videoEncodeParams);
			Result reconfigure( const VideoEncodeParams& videoEncodeParams);
			Result encode(const uint8_t* tagData, size_t tagDataSize, bool forceIDR = false);
			Result process(const uint8_t* tagData, size_t tagDataSize, bool forceIDR = false);
			Result release();

			avs::EncoderStats getEncoderStats() const;

			static Result getEncodeCapabilities( const VideoEncodeParams& videoEncodeParams, avs::EncodeCapabilities& capabilities);

			Result deconfigure();
		protected:
			std::unique_ptr<avs::Pipeline> mPipeline;
			std::unique_ptr<avs::Surface> mInputSurface;
			std::unique_ptr<avs::Encoder> mEncoder;
			avs::IOInterface* mTagDataOutput = nullptr;
			void* inputSurfaceResource = nullptr;
			void* encoderSurfaceResource = nullptr;
			bool configured = false;
		};
	}
}
