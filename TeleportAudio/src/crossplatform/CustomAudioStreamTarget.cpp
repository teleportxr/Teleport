#include "CustomAudioStreamTarget.h"

namespace sca
{
	CustomAudioStreamTarget::CustomAudioStreamTarget(std::function<void(const uint8_t * data, size_t dataSize)> playCallback)
		: mPlayCallback(playCallback) {}

	CustomAudioStreamTarget::~CustomAudioStreamTarget()
	{
		
	}

	avs::Result CustomAudioStreamTarget::deconfigure()
	{
		return avs::Result::OK;
	}

	avs::Result CustomAudioStreamTarget::process(const void* buffer, size_t bufferSizeInBytes, avs::AudioPayloadType payloadType)
	{
		const auto data = (uint8_t*)(buffer);
		mPlayCallback(data, bufferSizeInBytes);

		return avs::Result::OK;
	}
}
