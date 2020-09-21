// (C) Copyright 2018-2020 Simul Software Ltd
#pragma once

#include "crossplatform/AudioPlayer.h"
#include <wrl.h>
#include <future>

interface IXAudio2;
interface IXAudio2MasteringVoice;
interface IXAudio2SourceVoice;

/*! A class to play audio from streams and files for PC
*/
class PC_AudioPlayer final : public sca::AudioPlayer
{
public:
	PC_AudioPlayer();
	~PC_AudioPlayer();

	sca::Result playStream(const uint8_t* data, size_t dataSize) override;

	sca::Result initializeAudioDevice() override;

	sca::Result configure(const sca::AudioParams& audioParams) override;

	sca::Result deconfigure() override;

private:
	sca::Result asyncInitializeAudioDevice();

	std::future<sca::Result> initResult;

	Microsoft::WRL::ComPtr<IXAudio2> device;						
	IXAudio2MasteringVoice* masteringVoice;
	IXAudio2SourceVoice* sourceVoice;

	std::unique_ptr<class VoiceCallback> voiceCallback;
};


