// (C) Copyright 2018-2020 Simul Software Ltd
#pragma once

#include "crossplatform/AudioPlayer.h"

/*! A class to play audio from streams and files for PC
*/
class AA_AudioPlayer final : public sca::AudioPlayer
{
public:
	AA_AudioPlayer();
	~AA_AudioPlayer();

	sca::Result playStream(const uint8_t* data, size_t dataSize) override;

	sca::Result initializeAudioDevice() override;

	sca::Result configure(const sca::AudioParams& audioParams) override;

	sca::Result deconfigure() override;

private:
	class AAudioStream* audioStream;
};


