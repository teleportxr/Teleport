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
		SCA_CERR << "AA_AudioPlayer: Audio player has already been initialized" << std::endl;
		return sca::Result::AudioPlayerAlreadyInitialized;
	}

	mInitialized = true;

	return sca::Result::OK;
}

sca::Result AA_AudioPlayer::configure(const sca::AudioSettings& audioSettings)
{
	if (mConfigured)
	{
		SCA_CERR << "AA_AudioPlayer: Audio player has already been configured." << std::endl;
		return sca::Result::AudioPlayerAlreadyConfigured;
	}

	AAudioStreamBuilder* builder;

	if (FAILED(AAudio_createStreamBuilder(&builder)))
	{
		SCA_CERR << "AA_AudioPlayer: Error occurred trying to create audio stream builder." << std::endl;
		return sca::Result::AudioDeviceInitializationError;
	}
	AAudioStreamBuilder_setPerformanceMode(builder, AAUDIO_PERFORMANCE_MODE_LOW_LATENCY);

	AAudioStreamBuilder_setSharingMode(builder, AAUDIO_SHARING_MODE_EXCLUSIVE);
	AAudioStreamBuilder_setSampleRate(builder, audioSettings.sampleRate);
	AAudioStreamBuilder_setChannelCount(builder, audioSettings.numChannels);
	AAudioStreamBuilder_setFormat(builder, AAUDIO_FORMAT_PCM_FLOAT);

	auto result = sca::Result::OK;

	if (FAILED(AAudioStreamBuilder_openStream(builder, &mAudioStream)))
	{
		SCA_CERR << "AA_AudioPlayer: Error occurred trying to open audio stream." << std::endl;
		result = sca::Result::AudioStreamCreationError;
	}

	// Builder no longer needed after stream is created because we don't need any more streams
	if (FAILED(AAudioStreamBuilder_delete(builder)))
	{
		SCA_CERR << "AA_AudioPlayer: Error occurred trying to delete audio stream builder." << std::endl;
		if (result)
			result = sca::Result::AudioResourceDeletionError;
	}

	if (!result)
	{
		return result;
	}

	mAudioSettings = audioSettings;
	mConfigured = true;

	return result;
}

sca::Result AA_AudioPlayer::playStream(const uint8_t* data, size_t dataSize)
{
	if (!mInitialized)
	{
		SCA_CERR << "AA_AudioPlayer: Can't play audio stream because the audio player has not been initialized." << std::endl;
		return sca::Result::AudioPlayerNotInitialized;
	}

	if (!mConfigured)
	{
		SCA_CERR << "AA_AudioPlayer: Can't play audio stream because the audio player has not been configured."  << std::endl;
		return sca::Result::AudioPlayerNotConfigured;
	}

	int32_t numFrames = (int32_t)dataSize / (audioSettings.bitsPerSample * audioSettings.numChannels);
	if(FAILED(AAudioStream_write(mAudioStream, (const void*)data, numFrames, 100000)))
	{
		return sca::Result::AudioWriteError;
	}

	return sca::Result::OK;
}

sca::Result AA_AudioPlayer::startRecording(std::function<void(const uint8_t * data, size_t dataSize)> recordingCallback)
{
	if (!mInitialized)
	{
		SCA_CERR << "AA_AudioPlayer: Can't record audio because the audio player has not been initialized." << std::endl;
		return sca::Result::AudioPlayerNotInitialized;
	}

	if (!mConfigured)
	{
		SCA_CERR << "AA_AudioPlayer: Can't record audio because the audio player has not been configured." << std::endl;
		return sca::Result::AudioPlayerNotConfigured;
	}

	if (!mRecordingAllowed)
	{
		SCA_CERR << "AA_AudioPlayer: The user has not granted permission to record audio." << std::endl;
		return sca::Result::AudioRecordingNotPermitted;
	}

	if (mRecording)
	{
		SCA_CERR << "AA_AudioPlayers: Already recording." << std::endl;
		return sca::Result::OK;
	}

	mRecording = true;

	return sca::Result::OK;
}

// Not used because audio is processed asynchronously.
sca::Result AA_AudioPlayer::processRecordedAudio()
{
	return sca::Result::OK;
}

sca::Result AA_AudioPlayer::stopRecording()
{
	if (!mRecording)
	{
		SCA_CERR << "AA_AudioPlayer: Not recording." << std::endl;
		return sca::Result::OK;
	}

	mRecording = false;

	return sca::Result::OK;
}

sca::Result AA_AudioPlayer::deconfigure()
{
	if (!mConfigured)
	{
		SCA_CERR << "AA_AudioPlayer: Can't deconfigure audio player because it is not configured." << std::endl;
		return sca::Result::AudioPlayerNotConfigured;
	}

	if (FAILED(AAudioStream_close(mAudioStream)))
	{
		SCA_CERR << "AA_AudioPlayer: Error occurred trying to close audio stream." << std::endl;
		return sca::Result::AudioCloseStreamError;
	}

	mRecordingAllowed = false;

	mConfigured = false;

	mAudioSettings = {};

	return sca::Result::OK;
}

void AA_AudioPlayer::onAudioProcessed()
{

}



