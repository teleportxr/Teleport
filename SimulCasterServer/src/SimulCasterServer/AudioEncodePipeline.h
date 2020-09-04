#pragma once

#include <memory>
#include "CasterContext.h"
#include "CasterSettings.h"
#include "AudioEncoder.h"

// Forward declare so classes that include don't have to know about them
namespace avs
{
	class Pipeline;
	class AudioEncoder;
}

namespace SCServer
{
	struct AudioParams
	{
		avs::AudioCodec codec = avs::AudioCodec::PCM;
		uint32_t sampleRate = 44100;
		uint32_t numChannels = 2;
	};

	class AudioEncodePipeline
	{
	public:
		AudioEncodePipeline() = default;
		virtual ~AudioEncodePipeline();

		Result initialize(const CasterSettings& settings, const AudioParams& audioParams, avs::Node* output);
		Result process(const uint8_t* data, size_t dataSize);

	private:
		std::unique_ptr<AudioEncoder> encoderBackend;
		std::unique_ptr<avs::Pipeline> pipeline;
		std::unique_ptr<avs::AudioEncoder> encoder;
	};
}