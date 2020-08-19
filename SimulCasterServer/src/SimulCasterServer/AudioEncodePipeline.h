#pragma once

#include <memory>
#include "CasterContext.h"
#include "CasterSettings.h"

// Forward declare so classes that include don't have to know about them
namespace avs
{
	class Pipeline;
	class AudioEncoder;
}

namespace SCServer
{
	struct AudioEncodeParams
	{
		avs::AudioCodec codec = avs::AudioCodec::PCM;
	};

	class AudioEncodePipeline
	{
	public:
		AudioEncodePipeline() = default;
		virtual ~AudioEncodePipeline();

		Result initialize(const CasterSettings& settings, const AudioEncodeParams& audioEncodeParams, avs::Node* output);
		Result process(const uint8_t* data, size_t dataSize);

	private:
		std::unique_ptr<class AudioEncoder> encoderBackend;
		std::unique_ptr<avs::Pipeline> pipeline;
		std::unique_ptr<avs::AudioEncoder> encoder;
	};
}