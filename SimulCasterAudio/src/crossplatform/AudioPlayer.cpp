// (C) Copyright 2018-2020 Simul Software Ltd

#include "AudioPlayer.h"

namespace sca
{
	AudioPlayer::AudioPlayer()
		: initialized(false) {}

	AudioPlayer::~AudioPlayer() {}

	Result AudioPlayer::initialize(const AudioParams& audioParams)
	{
		if (initialized)
		{
			return Result::AudioPlayerAlreadyInitialized;
		}

		this->audioParams = audioParams;
		initialized = true;

		return Result::OK;
	}
}


