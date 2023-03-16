#include "AudioStreamTarget.h"
#include "AudioPlayer.h"

using namespace teleport::audio;
	AudioStreamTarget::AudioStreamTarget(AudioPlayer* ap)
		: player(ap) {}

	AudioStreamTarget::~AudioStreamTarget() 
	{
		
	}

	avs::Result AudioStreamTarget::deconfigure()
	{
		if (!player)
		{
			return avs::Result::AudioTargetBackend_NullAudioPlayer;
		}

		if (!player->deconfigure())
		{
			return avs::Result::AudioTargetBackend_PlayerDeconfigurationError;
		}

		return avs::Result::OK;
	}

	avs::Result AudioStreamTarget::process(const void* buffer, size_t bufferSizeInBytes, avs::AudioPayloadType )
	{
		const auto data = (uint8_t*)(buffer);
		auto result = player->playStream(data, bufferSizeInBytes);
		if (!result)
		{
			return avs::Result::AudioTargetBackend_AudioProcessingError;
		}
		return avs::Result::OK;
	}