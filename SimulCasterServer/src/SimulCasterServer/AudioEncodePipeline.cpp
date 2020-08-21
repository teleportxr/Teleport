#include "AudioEncodePipeline.h"
#include "ErrorHandling.h"
#include <iostream>
#include <algorithm>
#include <set>

#include <libavstream/platforms/platform_windows.hpp>
#include <libavstream/libavstream.hpp>
#include <libavstream/common.hpp>
#include <libavstream/audio/audio_interface.h>
#include "AudioEncoder.h"


namespace SCServer
{
	AudioEncodePipeline::~AudioEncodePipeline()
	{
		
	}

	Result AudioEncodePipeline::initialize(const CasterSettings& settings, const AudioEncodeParams& audioEncodeParams, avs::Node* output)
	{
		avs::AudioEncoderParams encoderParams;
		encoderParams.codec = audioEncodeParams.codec;

		encoderBackend.reset(new AudioEncoder(&settings));
		pipeline.reset(new avs::Pipeline);
		encoder.reset(new avs::AudioEncoder(encoderBackend.get()));

		if (!encoder->configure(encoderParams))
		{
			std::cout << "Failed to configure audio encoder node \n";
			return Result::EncoderNodeConfigurationError;
		}

		if (!pipeline->link({ encoder.get(), output }))
		{
			std::cout << "Error configuring the audio encoding pipeline \n";
			return Result::PipelineConfigurationError;
		}

		return Result::OK;
	}

	Result AudioEncodePipeline::process(const uint8_t* data, size_t dataSize)
	{
		if (!pipeline)
		{
			std::cout << "Error: audio encode pipeline not initialized \n";
			return Result::PipelineNotInitialized;
		}

		avs::Result result = pipeline->process();

		if (!result)
		{
			TELEPORT_CERR << "Audio encode pipeline processing encountered an error \n";
			return Result::PipelineProcessingError;
		}

		result = encoder->writeOutput(data, dataSize);

		if (!result)
		{
			TELEPORT_CERR << "Audio encode pipeline encountered an error trying to write output \n";
			return Result::PipelineWriteOutputError;
		}

		return Result::OK;
	}
}