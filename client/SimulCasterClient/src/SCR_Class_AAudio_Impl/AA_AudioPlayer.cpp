// (C) Copyright 2018-2020 Simul Software Ltd

#include "AA_AudioPlayer.h"
#include <chrono>
#include <thread>

#define FAILED(r)      (((aaudio_result_t)(r)) != AAUDIO_OK)

AA_AudioPlayer::AA_AudioPlayer()
{
	
}

AA_AudioPlayer::~AA_AudioPlayer()
{
	if (mConfigured)
	{
		deconfigure();
	}
}

sca::Result AA_AudioPlayer::initializeAudioDevice()
{
	if (mInitialized)
	{
		SCA_CERR("AA_AudioPlayer: Audio player has already been initialized");
		return sca::Result::AudioPlayerAlreadyInitialized;
	}

	mInitialized = true;

	return sca::Result::OK;
}

sca::Result AA_AudioPlayer::configure(const sca::AudioParams& audioParams)
{
	if (mConfigured)
	{
		SCA_CERR("AA_AudioPlayer: Audio player has already been configured.");
		return sca::Result::AudioPlayerAlreadyConfigured;
	}

	AAudioStreamBuilder* builder;

	if (FAILED(AAudio_createStreamBuilder(&builder)))
	{
		SCA_CERR("AA_AudioPlayer: Error occurred trying to create audio stream builder.");
		return sca::Result::AudioDeviceInitializationError;
	}
	AAudioStreamBuilder_setPerformanceMode(builder, AAUDIO_PERFORMANCE_MODE_LOW_LATENCY);

	AAudioStreamBuilder_setSharingMode(builder, AAUDIO_SHARING_MODE_EXCLUSIVE);
	AAudioStreamBuilder_setSampleRate(builder, audioParams.sampleRate);
	AAudioStreamBuilder_setChannelCount(builder, audioParams.numChannels);
	AAudioStreamBuilder_setFormat(builder, AAUDIO_FORMAT_PCM_FLOAT);

	auto result = sca::Result::OK;

	if (FAILED(AAudioStreamBuilder_openStream(builder, &mAudioStream)))
	{
		SCA_CERR("AA_AudioPlayer: Error occurred trying to open audio stream.");
		result = sca::Result::AudioStreamCreationError;
	}

	// Builder no longer needed after stream is created because we don't need any more streams
	if (FAILED(AAudioStreamBuilder_delete(builder)))
	{
		SCA_CERR("AA_AudioPlayer: Error occurred trying to delete audio stream builder.");
		if (result)
			result = sca::Result::AudioResourceDeletionError;
	}

	if (!result)
	{
		return result;
	}

	mAudioParams = audioParams;
	mConfigured = true;

	return result;
}

sca::Result AA_AudioPlayer::deconfigure()
{
	if (!mConfigured)
	{
		SCA_CERR("AA_AudioPlayer: Can't deconfigure audio player because it is not configured.");
		return sca::Result::AudioPlayerNotConfigured;
	}

	if (FAILED(AAudioStream_close(mAudioStream)))
	{
		SCA_CERR("AA_AudioPlayer: Error occurred trying to close audio stream.");
		return sca::Result::AudioCloseStreamError;
	}

	mConfigured = false;

	mAudioParams = {};

	return sca::Result::OK;
}

sca::Result AA_AudioPlayer::playStream(const uint8_t* data, size_t dataSize)
{
	if (!mInitialized)
	{
		SCA_CERR("AA_AudioPlayer: Can't play audio stream because the audio player has not been initialized.");
		return sca::Result::AudioPlayerNotInitialized;
	}

	if (!mConfigured)
	{
		SCA_CERR("AA_AudioPlayer: Can't play audio stream because the audio player has not been configured.");
		return sca::Result::AudioPlayerNotConfigured;
	}

	int32_t numFrames = (int32_t)dataSize / (audioParams.bitsPerSample * audioParams.numChannels);
	if(FAILED(AAudioStream_write(mAudioStream, (const void*)data, numFrames, 100000)))
	{
		return sca::Result::AudioWriteError;
	}

	return sca::Result::OK;
}



