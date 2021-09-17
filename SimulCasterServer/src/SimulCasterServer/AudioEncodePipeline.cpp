#include "AudioEncodePipeline.h"
#include "ErrorHandling.h"
#include <iostream>
#include <algorithm>
#include <set>

#include <libavstream/platforms/platform_windows.hpp>
#include <libavstream/libavstream.hpp>
#include <libavstream/common.hpp>
#include <libavstream/audio/audio_interface.h>

namespace teleport
{
	AudioEncodePipeline::~AudioEncodePipeline()
	{
		
	}

	Result AudioEncodePipeline::initialize(const CasterSettings& settings, const AudioParams& audioParams, avs::PipelineNode* output)
	{
		avs::AudioEncoderParams encoderParams;
		encoderParams.codec = audioParams.codec;

		encoder.reset(new avs::AudioEncoder(new AudioEncoder(&settings)));
		pipeline.reset(new avs::Pipeline);

		if (!encoder->configure(encoderParams))
		{
			TELEPORT_CERR << "Failed to configure audio encoder node \n";
			return Result::Code::EncoderNodeConfigurationError;
		}

		if (!pipeline->link({ encoder.get(), output }))
		{
			TELEPORT_CERR << "Error configuring the audio encoding pipeline \n";
			return Result::Code::PipelineConfigurationError;
		}

		return Result::Code::OK;
	}

	Result AudioEncodePipeline::process(const uint8_t* data, size_t dataSize)
	{
		if (!pipeline)
		{
			TELEPORT_CERR << "Error: audio encode pipeline not initialized \n";
			return Result::Code::PipelineNotInitialized;
		}

		avs::Result result = pipeline->process();

		if (!result)
		{
			TELEPORT_CERR << "Audio encode pipeline processing encountered an error \n";
			return Result::Code::PipelineProcessingError;
		}

		result = encoder->writeOutput(data, dataSize);

		if (!result)
		{
			TELEPORT_CERR << "Audio encode pipeline encountered an error trying to write output \n";
			return Result::Code::PipelineWriteOutputError;
		}

		return Result::Code::OK;
	}
}