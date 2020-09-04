#include "AudioStreamTarget.h"
#include "AudioPlayer.h"

namespace sca
{
	AudioStreamTarget::AudioStreamTarget(AudioPlayer* ap)
		: player(ap) {}

	AudioStreamTarget::~AudioStreamTarget() 
	{
		SAFE_DELETE(player);
	}

	avs::Result AudioStreamTarget::process(const void* buffer, size_t bufferSizeInBytes, avs::AudioPayloadType payloadType)
	{
		const auto data = (uint8_t*)(buffer);
		auto result = player->playStream(data, bufferSizeInBytes);
		if (!result)
		{
			return avs::Result::AudioTargetBackend_AudioProcessingError;
		}
		return avs::Result::OK;
	}
}
