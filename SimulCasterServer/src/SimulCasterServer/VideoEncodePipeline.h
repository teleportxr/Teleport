#pragma once

#include <memory>
#include "CasterContext.h"
#include "CasterSettings.h"

// Forward declare so classes that include don't have to know about them
namespace avs
{
	class Pipeline;
	class Encoder;
	class Surface;
}

namespace SCServer
{
	struct VideoEncodeParams
	{
		int32_t encodeWidth = 0;
		int32_t encodeHeight = 0;
		GraphicsDeviceType deviceType;
		void* deviceHandle = nullptr;
		void* inputSurfaceResource = nullptr;
	};

	class VideoEncodePipeline
	{
	public:
		VideoEncodePipeline() = default;
		virtual ~VideoEncodePipeline();

		Result initialize(const CasterSettings& settings, const VideoEncodeParams& videoEncodeParams, avs::Node* output);
		Result reconfigure(const CasterSettings& settings, const VideoEncodeParams& videoEncodeParams);
		Result process(const uint8_t* extraData, size_t extraDataSize, bool forceIDR = false);
		Result release();

	private:
		std::unique_ptr<avs::Pipeline> pipeline;
		std::unique_ptr<avs::Surface> inputSurface;
		std::unique_ptr<avs::Encoder> encoder;
	};
}