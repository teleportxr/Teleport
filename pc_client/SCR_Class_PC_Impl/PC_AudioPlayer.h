// (C) Copyright 2018-2020 Simul Software Ltd
#pragma once

#include "crossplatform/AudioPlayer.h"
#include <wrl.h>

interface IXAudio2;
interface IXAudio2MasteringVoice;

/*! A class to play audio from streams and files for PC
*/
class PC_AudioPlayer final : public sca::AudioPlayer
{
public:
	PC_AudioPlayer();
	~PC_AudioPlayer();

	sca::Result playStream(const float* data, size_t dataSize, sca::AudioType audioType) override;

protected:
	sca::Result initalize() override;

private:
	Microsoft::WRL::ComPtr<IXAudio2> device;						// the main XAudio2 engine
	IXAudio2MasteringVoice* masterVoice;							// a mastering voice
};


