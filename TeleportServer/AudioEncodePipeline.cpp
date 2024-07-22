#include "AudioEncodePipeline.h"
#include "TeleportCore/ErrorHandling.h"
#include <iostream>
#include <algorithm>
#include <set>

#include <libavstream/platforms/this_platform.h>
#include <libavstream/libavstream.hpp>
#include <libavstream/common.hpp>
#include <libavstream/audio/audio_interface.h>

using namespace teleport;
using namespace server;

AudioEncodePipeline::~AudioEncodePipeline()
{
	
}

Result AudioEncodePipeline::initialize(const ServerSettings& settings, const AudioSettings& audioSettings, avs::PipelineNode* output)
{
	avs::AudioEncoderParams encoderParams;
	encoderParams.codec = audioSettings.codec;

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

Result AudioEncodePipeline::configure(const ServerSettings& serverSettings, const AudioSettings& audioSettings, avs::PipelineNode* audioQueue)
{
	if (configured)
	{
		TELEPORT_CERR << "Audio encode pipeline already configured." << "\n";
		return Result::Code::EncoderAlreadyConfigured;
	}

	Result result = AudioEncodePipeline::initialize(serverSettings, audioSettings, audioQueue);
	if (result)
	{
		configured = true;
	}
	return result;
}

Result AudioEncodePipeline::sendAudio(const uint8_t* data, size_t dataSize)
{
	if (!configured)
	{
		TELEPORT_CERR << "Audio encoder can not encode because it has not been configured." << "\n";
		return Result::Code::EncoderNotConfigured;
	}

	return AudioEncodePipeline::process(data, dataSize);
}