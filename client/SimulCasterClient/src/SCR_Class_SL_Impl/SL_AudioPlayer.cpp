// (C) Copyright 2018-2020 Simul Software Ltd

#include "SL_AudioPlayer.h"
#include <chrono>
#include <thread>

//#define FAILED(r)      (((aaudio_result_t)(r)) != AAUDIO_OK)

SL_AudioPlayer::SL_AudioPlayer()
{
	
}

SL_AudioPlayer::~SL_AudioPlayer()
{
	if (configured)
		deconfigure();
}

sca::Result SL_AudioPlayer::initializeAudioDevice()
{
	if (initialized)
	{
		SCA_CERR("Audio player has already been initialized");
		return sca::Result::AudioPlayerAlreadyInitialized;
	}

	initialized = true;

	return sca::Result::OK;
}

sca::Result SL_AudioPlayer::configure(const sca::AudioParams& audioParams)
{
	if (configured)
	{
		SCA_CERR("Audio player has already been configured.");
		return sca::Result::AudioPlayerAlreadyConfigured;
	}

	sca::Result result = sca::Result::OK;

	// TODO: Configure SLES

	this->audioParams = audioParams;
	configured = true;

	return result;
}

sca::Result SL_AudioPlayer::deconfigure()
{
	if (!configured)
	{
		SCA_CERR("Can't deconfigure audio player because it is not configured.");
		return sca::Result::AudioPlayerNotConfigured;
	}

	// TODO: Deconfigure SLES

	configured = false;

	audioParams = {};

	return sca::Result::OK;
}

sca::Result SL_AudioPlayer::playStream(const uint8_t* data, size_t dataSize)
{
	if (!initialized)
	{
		SCA_CERR("Can't play audio stream because the audio player has not been initialized.");
		return sca::Result::AudioPlayerNotInitialized;
	}

	if (!configured)
	{
		SCA_CERR("Can't play audio stream because the audio player has not been configured.");
		return sca::Result::AudioPlayerNotConfigured;
	}

	int32_t numFrames = (int32_t)dataSize / (audioParams.bitsPerSample * audioParams.numChannels);

	// TODO: Write audio data using SLES
	//if(FAILED(AAudioStream_write(audioStream, (const void*)data, numFrames, 100000)))
	//{
		//return sca::Result::AudioWriteError;
	//}

	return sca::Result::OK;
}



