#pragma once

#include <memory>
#include "CasterContext.h"
#include "ServerSettings.h"
#include "UnityPlugin/PluginGraphics.h"

// Forward declare so classes that include don't have to know about them
namespace avs
{
	class Pipeline;
	class Encoder;
	class Surface;
}

namespace teleport
{
	struct VideoEncodeParams
	{
		int32_t encodeWidth = 0;
		int32_t encodeHeight = 0;
		GraphicsDeviceType deviceType;
		void* deviceHandle = nullptr;
		void* inputSurfaceResource = nullptr;
	};
	
	//! Wrapper for the video encoding pipeline objects.
	class VideoEncodePipeline
	{
	public:
		VideoEncodePipeline() = default;
		virtual ~VideoEncodePipeline();

		Result initialize(const ServerSettings& settings, const VideoEncodeParams& videoEncodeParams, avs::PipelineNode* videoOutput, avs::IOInterface* tagDataOutput);
		
		Result configure(const ServerSettings &serverSettings,const VideoEncodeParams& videoEncodeParams, avs::Queue* colorQueue, avs::Queue* tagDataQueue);
		Result reconfigure1(const ServerSettings& settings, const VideoEncodeParams& videoEncodeParams);
		Result reconfigure(const ServerSettings &serverSettings,const VideoEncodeParams& videoEncodeParams);
		Result encode(const uint8_t* tagData, size_t tagDataSize, bool forceIDR = false);
		Result process(const uint8_t* tagData, size_t tagDataSize, bool forceIDR = false);
		Result release();

		avs::EncoderStats getEncoderStats() const;

		static Result getEncodeCapabilities(const ServerSettings& settings, const VideoEncodeParams& videoEncodeParams, avs::EncodeCapabilities& capabilities);
		
		Result deconfigure();
	protected:
		std::unique_ptr<avs::Pipeline> mPipeline;
		std::unique_ptr<avs::Surface> mInputSurface;
		std::unique_ptr<avs::Encoder> mEncoder;
		avs::IOInterface* mTagDataOutput = nullptr;
		void* inputSurfaceResource= nullptr;
		void* encoderSurfaceResource= nullptr;
		bool configured=false;
	};
}
