// (C) Copyright 2018-2020 Simul Software Ltd
#pragma once

#include <libavstream/node.hpp>
#include <libavstream/audio/audio_interface.h>


namespace sca
{
	class AudioPlayer;

	/*! A class to receive streamed audio and play it
	*/
	class AudioStreamTarget final : public avs::AudioTargetBackendInterface
	{
	public:
		AudioStreamTarget(AudioPlayer* ap);

		~AudioStreamTarget();

		avs::Result process(const void* buffer, size_t bufferSizeInBytes, avs::AudioPayloadType payloadType) override;

		AudioPlayer* GetAudioPlayer() const
		{
			return player;
		}

	private:
		AudioPlayer* player;
	};
}

