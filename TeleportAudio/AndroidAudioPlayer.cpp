// (C) Copyright 2018-2020 Simul Software Ltd

#include "AndroidAudioPlayer.h"
#include <chrono>
#include <thread>
#include <oboe/Oboe.h>

using namespace teleport::audio;

#pragma clang optimize off
namespace teleport
{
	namespace audio
	{
		struct AndroidAudioPlayer::Private:public oboe::AudioStreamDataCallback
		{
			oboe::AudioStreamBuilder builder;
			std::shared_ptr<oboe::AudioStream> mStream;
			std::vector<float> buffer;
			std::mutex bufferMutex;
			// take numFrames floats from the front of the buffer.
			oboe::DataCallbackResult onAudioReady(oboe::AudioStream* audioStream, void* audioData, int32_t numFrames)
			{
				std::lock_guard lock(bufferMutex);
				TELEPORT_INTERNAL_COUT("onAudioReady numFrames {0}\n",numFrames);
				// We requested AudioFormat::Float. So if the stream opens
			// we know we got the Float format.
				// If you do not specify a format then you should check what format
				// the stream has and cast to the appropriate type.
				auto* outputData = static_cast<float*>(audioData);
				// Generate random numbers (white noise) centered around zero.
			/*	const float amplitude = 0.2f;
				for (int i = 0; i < numFrames; ++i)
				{
					outputData[i] = ((float)drand48() - 0.5f) * 2 * amplitude;
				}*/
				// How many floats can we copy?
				int32_t ct = std::min(numFrames, (int32_t)buffer.size());
				// memory size to copy from buffer.
				size_t memsize = ct * sizeof(float);
				memcpy(outputData, buffer.data(), memsize);
				// if we didn't manage to fill the output buffer, fill the rest with zeroes.
				if (ct < numFrames)
					memset((void*)(outputData + ct), 0, numFrames * sizeof(float) - memsize);
				// move the data up by numFrames.
				memcpy(buffer.data(), (const void*)(buffer.data() + ct), buffer.size() - ct);
				buffer.resize(buffer.size() - ct);
				return oboe::DataCallbackResult::Continue;
			}
		};
	}
}

AndroidAudioPlayer::AndroidAudioPlayer()
{
	m_data = new Private;
}

AndroidAudioPlayer::~AndroidAudioPlayer()
{
	if (mConfigured)
	{
		deconfigure();
	}
	delete m_data;
}

Result AndroidAudioPlayer::initializeAudioDevice()
{
	if (mInitialized)
	{
		TELEPORT_INTERNAL_CERR("AndroidAudioPlayer: Audio player has already been initialized\n");
		return Result::AudioPlayerAlreadyInitialized;
	}

	mInitialized = true;

	return Result::OK;
}

Result AndroidAudioPlayer::configure(const AudioSettings& audioSettings)
{
	if (mConfigured)
	{
		TELEPORT_INTERNAL_CERR("AndroidAudioPlayer: Audio player has already been configured.\n");
		return Result::AudioPlayerAlreadyConfigured;
	}

	m_data->builder.setPerformanceMode( oboe::PerformanceMode::LowLatency);
	m_data->builder.setDirection(oboe::Direction::Output);
	m_data->builder.setSharingMode(oboe::SharingMode::Exclusive);
	m_data->builder.setSampleRate( audioSettings.sampleRate);
	m_data->builder.setChannelCount( audioSettings.numChannels);
	m_data->builder.setFormat(oboe::AudioFormat::Float);

	auto audioResult = Result::OK;
	m_data->builder.setDataCallback(m_data);
	// "openStream" ACTUALLY means "CREATE STREAM"!
	// This creates the oboe::AudioStream object and assigns the shared_ptr.
	oboe::Result result = m_data->builder.openStream(m_data->mStream);
	if (result!= oboe::Result::OK)
	{
		TELEPORT_INTERNAL_CERR("Failed to create stream. Error: {0}", oboe::convertToText(result));
		audioResult = Result::AudioStreamCreationError;
	}

	if (audioResult!=Result::OK)
	{
		return audioResult;
	}

	mAudioSettings = audioSettings;
	mConfigured = true;

	return audioResult;
}

Result AndroidAudioPlayer::playStream(const uint8_t* data, size_t dataSize)
{
	if (!mInitialized)
	{
		SCA_CERR << "AndroidAudioPlayer: Can't play audio stream because the audio player has not been initialized." << std::endl;
		return Result::AudioPlayerNotInitialized;
	}

	if (!mConfigured)
	{
		SCA_CERR << "AndroidAudioPlayer: Can't play audio stream because the audio player has not been configured."  << std::endl;
		return Result::AudioPlayerNotConfigured;
	}
	auto state=m_data->mStream->getState();
	if (state == oboe::StreamState::Open)
	{
		oboe::AudioFormat format = m_data->mStream->getFormat();
		TELEPORT_INTERNAL_COUT("AudioStream format is {0}", oboe::convertToText(format));
		oboe::Result result = m_data->mStream->requestStart();
		if (result != oboe::Result::OK)
		{
			TELEPORT_INTERNAL_CERR("mStream->requestStart Failed. Error: {0}", oboe::convertToText(result));
			return Result::AudioStreamCreationError;
		}
	}
	int32_t numFrames = (int32_t)dataSize / (mAudioSettings.bitsPerSample * mAudioSettings.numChannels);
	/*if(FAILED(AAudioStream_write(mAudioStream, (const void*)data, numFrames, 100000)))
	{
		return Result::AudioWriteError;
	}*/
	{
		std::lock_guard lock(m_data->bufferMutex);
	}
	TELEPORT_INTERNAL_COUT("playStream data size {0}\n", dataSize);
	return Result::OK;
}

Result AndroidAudioPlayer::startRecording(std::function<void(const uint8_t * data, size_t dataSize)> recordingCallback)
{
	if (!mInitialized)
	{
		SCA_CERR << "AndroidAudioPlayer: Can't record audio because the audio player has not been initialized." << std::endl;
		return Result::AudioPlayerNotInitialized;
	}

	if (!mConfigured)
	{
		SCA_CERR << "AndroidAudioPlayer: Can't record audio because the audio player has not been configured." << std::endl;
		return Result::AudioPlayerNotConfigured;
	}

	if (!mRecordingAllowed)
	{
		SCA_CERR << "AndroidAudioPlayer: The user has not granted permission to record audio." << std::endl;
		return Result::AudioRecordingNotPermitted;
	}

	if (mRecording)
	{
		SCA_CERR << "AndroidAudioPlayers: Already recording." << std::endl;
		return Result::OK;
	}

	mRecording = true;

	return Result::OK;
}

// Not used because audio is processed asynchronously.
Result AndroidAudioPlayer::processRecordedAudio()
{
	return Result::OK;
}

Result AndroidAudioPlayer::stopRecording()
{
	if (!mRecording)
	{
		SCA_CERR << "AndroidAudioPlayer: Not recording." << std::endl;
		return Result::OK;
	}

	mRecording = false;

	return Result::OK;
}

Result AndroidAudioPlayer::deconfigure()
{
	if (!mConfigured)
	{
		SCA_CERR << "AndroidAudioPlayer: Can't deconfigure audio player because it is not configured." << std::endl;
		return Result::AudioPlayerNotConfigured;
	}
	auto result=m_data->mStream->requestStop();
	if (result != oboe::Result::OK)
	{
		TELEPORT_INTERNAL_CERR("Failed to stop stream. Error: {0}", oboe::convertToText(result));
	}

	mRecordingAllowed = false;

	mConfigured = false;

	mAudioSettings = {};

	return Result::OK;
}

void AndroidAudioPlayer::onAudioProcessed()
{

}



