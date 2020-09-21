// (C) Copyright 2018-2020 Simul Software Ltd

#include "AA_AudioPlayer.h"
#include <chrono>
#include <thread>
#include <aaudio/AAudio.h>


AA_AudioPlayer::AA_AudioPlayer()
{
	
}

AA_AudioPlayer::~AA_AudioPlayer()
{
	if (configured)
		deconfigure();
}

sca::Result AA_AudioPlayer::initializeAudioDevice()
{
	if (initialized)
	{
		return sca::Result::AudioPlayerAlreadyInitialized;
	}

	initialized = true;

	return sca::Result::OK;
}

sca::Result AA_AudioPlayer::configure(const sca::AudioParams& audioParams)
{
	if (configured)
	{
		SCA_COUT("Audio player has already been configured.");
		return sca::Result::AudioPlayerAlreadyConfigured;
	}

	AAudioStreamBuilder* builder;
	aaudio_result_t r = AAudio_createStreamBuilder(&builder);
	if (r != AAUDIO_OK)
	{
		SCA_COUT("Error occurred trying to create audio stream builder.");
		return sca::Result::AudioStreamBuilderCreationError;
	}
	AAudioStreamBuilder_setPerformanceMode(builder, AAUDIO_PERFORMANCE_MODE_LOW_LATENCY);

	AAudioStreamBuilder_setSharingMode(builder, AAUDIO_SHARING_MODE_EXCLUSIVE);
	AAudioStreamBuilder_setSampleRate(builder, audioParams.sampleRate);
	AAudioStreamBuilder_setChannelCount(builder, audioParams.numChannels);
	AAudioStreamBuilder_setFormat(builder, AAUDIO_FORMAT_PCM_FLOAT);

	r = AAudioStreamBuilder_openStream(builder, &audioStream);

	// Builder no longer neeeded after stream is created because we don't need any more streams
	aaudio_result_t dr = AAudioStreamBuilder_delete(builder);
	if (dr != AAUDIO_OK)
	{
		SCA_COUT("Error occurred trying to delete audio stream builder.");
		return sca::Result::AudioStreamBuilderDeletionError;
	}

	if (r != AAUDIO_OK)
	{
		SCA_COUT("Error occurred trying to open audio stream.");
		return sca::Result::AudioOpenStreamError;
	}
	
	this->audioParams = audioParams;
	configured = true;
}

sca::Result AA_AudioPlayer::deconfigure()
{
	if (!configured)
	{
		SCA_COUT("Can't deconfigure audio player because it is not configured.");
		return sca::Result::AudioPlayerNotConfigured;
	}

	aaudio_result_t r = AAudioStream_close(audioStream);

	if (r != AAUDIO_OK)
	{
		SCA_COUT("Error occurred trying to close audio stream.");
		return sca::Result::AudioCloseStreamError;
	}

	//r = AAudioStream_release(audioStream);

	//if (r != AAUDIO_OK)
	//{
		//SCA_COUT("Error occurred trying to release audio stream.");
		//return sca::Result::AudioReleaseStreamError;
	//}

	configured = false;

	audioParams = {};

	return sca::Result::OK;
}

sca::Result AA_AudioPlayer::playStream(const uint8_t* data, size_t dataSize)
{
	if (!initialized)
	{
		SCA_COUT("Can't play audio stream because the audio player has not been initialized.");
		return sca::Result::AudioPlayerNotInitialized;
	}

	if (!configured)
	{
		SCA_COUT("Can't play audio stream because the audio player has not been configured.");
		return sca::Result::AudioPlayerNotConfigured;
	}

	int32_t numFrames = dataSize / (audioParams.bitsPerSample * audioParams.numChannels);
	AAudioStream_write(audioStream, (void*)data, numFrames, 100000);

	return sca::Result::OK;
}



