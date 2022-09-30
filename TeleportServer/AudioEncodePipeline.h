#pragma once

#include <memory>
#include "CasterContext.h"
#include "ServerSettings.h"
#include "AudioEncoder.h"

// Forward declare so classes that include don't have to know about them
namespace avs
{
	class Pipeline;
	class PipelineNode;
	class AudioEncoder;
}

namespace teleport
{
	struct AudioSettings
	{
		avs::AudioCodec codec = avs::AudioCodec::PCM;
		uint32_t sampleRate = 44100;
		uint32_t bitsPerSample = 16;
		uint32_t numChannels = 2;
	};
	//! A wrapper for an avs::AudioEncoder and an avs::Pipeline.
	class AudioEncodePipeline
	{
	public:
		AudioEncodePipeline() = default;
		virtual ~AudioEncodePipeline();

		Result initialize(const ServerSettings& settings, const AudioSettings& audioSettings, avs::PipelineNode* output);
		Result process(const uint8_t* data, size_t dataSize);
		
		Result configure(const ServerSettings& serverSettings, const AudioSettings& audioSettings, avs::Queue* audioQueue);

		Result sendAudio(const uint8_t* data, size_t dataSize);

		bool isConfigured() const
		{
			return configured;
		}
	private:
		std::unique_ptr<avs::AudioEncoder> encoder;
		std::unique_ptr<avs::Pipeline> pipeline;
		bool configured;
	};


};