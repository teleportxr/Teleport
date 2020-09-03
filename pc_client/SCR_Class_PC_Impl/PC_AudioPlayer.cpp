// (C) Copyright 2018-2020 Simul Software Ltd

#include "PC_AudioPlayer.h"
#include "xaudio2.h"
#include "Platform/CrossPlatform/Macros.h"

PC_AudioPlayer::PC_AudioPlayer()
{

}


PC_AudioPlayer::~PC_AudioPlayer()
{
	SAFE_DELETE(masterVoice);
}

sca::Result PC_AudioPlayer::initalize()
{
	HRESULT hr = S_OK;

	// Get an interface to the main XAudio2 device
	hr = XAudio2Create(device.GetAddressOf());
	if (FAILED(hr))
		return sca::Result::AudioDeviceInitializationError;

	// Create master voice
	hr = device->CreateMasteringVoice(&masterVoice);
	if (FAILED(hr))
		return sca::Result::AudioMasteringVoiceCreationError;

	return sca::Result::OK;
}

sca::Result PC_AudioPlayer::playStream(const float* data, size_t dataSize, sca::AudioType audioType)
{
	return sca::Result::OK;
}



