// (C) Copyright 2018-2020 Simul Software Ltd
#pragma once

#include <libavstream/interfaces.hpp>


namespace sca
{
	class AudioPlayer;
	/*! A class to receive streamed audio and play it
	*/
	class AudioStreamTarget final : public avs::AudioTargetInterface
	{
	public:
		AudioStreamTarget(AudioPlayer* ap);

		~AudioStreamTarget() = default;

		avs::Result process(const void* buffer, size_t bufferSizeInBytes, avs::AudioPayloadType payloadType) override;

	private:
		AudioPlayer* player;
	};
}

