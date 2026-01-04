// (C) Copyright 2018-2024 Simul Software Ltd

#include "LinuxAudioPlayer.h"
#include "TeleportCore/ErrorHandling.h"
#include <chrono>
#include <thread>
#include <pulse/simple.h>
#include <pulse/error.h>

using namespace teleport::audio;

LinuxAudioPlayer::LinuxAudioPlayer()
{
	mRecordingAllowed = true;
}

LinuxAudioPlayer::~LinuxAudioPlayer()
{
	if (mConfigured)
	{
		deconfigure();
	}
}

Result LinuxAudioPlayer::initializeAudioDevice()
{
	if (mInitialized)
	{
		return Result::AudioPlayerAlreadyInitialized;
	}
	mInitResult = std::async(std::launch::async, &LinuxAudioPlayer::asyncInitializeAudioDevice, this);
	return Result::OK;
}

Result LinuxAudioPlayer::asyncInitializeAudioDevice()
{
	// PulseAudio initialization is deferred to configure() where we know the audio format
	return Result::OK;
}

Result LinuxAudioPlayer::configure(const AudioSettings& audioSettings)
{
	if (mConfigured)
	{
		TELEPORT_COUT << "LinuxAudioPlayer: Audio player has already been configured." << std::endl;
		return Result::AudioPlayerAlreadyConfigured;
	}

	if (!mInitialized)
	{
		Result res = initializeAudioDevice();
		if (res == Result::OK)
		{
			// Wait for async initialization
			Result result = mInitResult.get();
			if (!result && result != Result::NoAudioInputDeviceFound)
			{
				return result;
			}
			mInputDeviceAvailable = (result == Result::OK);
		}
		else if (res != Result::AudioPlayerAlreadyInitialized)
		{
			return res;
		}
		mInitialized = true;
	}

	// Configure PulseAudio sample spec
	pa_sample_spec sampleSpec;
	sampleSpec.channels = static_cast<uint8_t>(audioSettings.numChannels);
	sampleSpec.rate = audioSettings.sampleRate;
	
	// Determine format based on bits per sample
	if (audioSettings.bitsPerSample == 32)
	{
		sampleSpec.format = PA_SAMPLE_FLOAT32LE;
	}
	else if (audioSettings.bitsPerSample == 16)
	{
		sampleSpec.format = PA_SAMPLE_S16LE;
	}
	else
	{
		TELEPORT_CERR << "LinuxAudioPlayer: Unsupported bits per sample: " << audioSettings.bitsPerSample << std::endl;
		return Result::AudioStreamConfigurationError;
	}

	int error;
	mPlaybackStream = pa_simple_new(
		nullptr,                // Use default server
		"TeleportClient",       // Application name
		PA_STREAM_PLAYBACK,     // Stream direction
		nullptr,                // Use default device
		"Audio Playback",       // Stream description
		&sampleSpec,            // Sample format
		nullptr,                // Use default channel map
		nullptr,                // Use default buffering attributes
		&error
	);

	if (!mPlaybackStream)
	{
		TELEPORT_CERR << "LinuxAudioPlayer: Failed to create playback stream: " << pa_strerror(error) << std::endl;
		return Result::AudioStreamCreationError;
	}

	mAudioSettings = audioSettings;
	mConfigured = true;

	return Result::OK;
}

Result LinuxAudioPlayer::playStream(const uint8_t* data, size_t dataSize)
{
	if (!mInitialized)
	{
		TELEPORT_CERR << "LinuxAudioPlayer: Can't play audio stream because the audio player has not been initialized." << std::endl;
		return Result::AudioPlayerNotInitialized;
	}
	if (!mConfigured)
	{
		static bool done = false;
		if (!done)
		{
			TELEPORT_CERR << "LinuxAudioPlayer: Can't play audio stream because the audio player has not been configured." << std::endl;
			done = true;
		}
		return Result::AudioPlayerNotConfigured;
	}

	if (!mPlaybackStream)
	{
		return Result::AudioPlayerNotInitialized;
	}

	int error;
	if (pa_simple_write(mPlaybackStream, data, dataSize, &error) < 0)
	{
		TELEPORT_CERR << "LinuxAudioPlayer: Failed to write audio data: " << pa_strerror(error) << std::endl;
		return Result::AudioWriteError;
	}

	return Result::OK;
}

Result LinuxAudioPlayer::startRecording(std::function<void(const uint8_t* data, size_t dataSize)> recordingCallback)
{
	if (!mInitialized)
	{
		TELEPORT_CERR << "LinuxAudioPlayer: Can't record audio because the audio player has not been initialized." << std::endl;
		return Result::AudioPlayerNotInitialized;
	}

	if (!mConfigured)
	{
		TELEPORT_CERR << "LinuxAudioPlayer: Can't record audio because the audio player has not been configured." << std::endl;
		return Result::AudioPlayerNotConfigured;
	}

	if (!mRecordingAllowed)
	{
		TELEPORT_CERR << "LinuxAudioPlayer: The user has not granted permission to record audio." << std::endl;
		return Result::AudioRecordingNotPermitted;
	}

	if (mRecording)
	{
		TELEPORT_COUT << "LinuxAudioPlayer: Already recording." << std::endl;
		return Result::OK;
	}

	mRecordingCallback = recordingCallback;
	mCaptureRunning = true;
	mCaptureThread = std::thread(&LinuxAudioPlayer::captureThreadFunc, this);
	mRecording = true;

	return Result::OK;
}

void LinuxAudioPlayer::captureThreadFunc()
{
	pa_sample_spec sampleSpec;
	sampleSpec.channels = static_cast<uint8_t>(mAudioSettings.numChannels);
	sampleSpec.rate = mAudioSettings.sampleRate;

	if (mAudioSettings.bitsPerSample == 32)
	{
		sampleSpec.format = PA_SAMPLE_FLOAT32LE;
	}
	else
	{
		sampleSpec.format = PA_SAMPLE_S16LE;
	}

	int error;
	pa_simple* captureStream = pa_simple_new(
		nullptr,
		"TeleportClient",
		PA_STREAM_RECORD,
		nullptr,
		"Audio Capture",
		&sampleSpec,
		nullptr,
		nullptr,
		&error
	);

	if (!captureStream)
	{
		TELEPORT_CERR << "LinuxAudioPlayer: Failed to create capture stream: " << pa_strerror(error) << std::endl;
		mCaptureRunning = false;
		return;
	}

	const size_t bufferSize = (mAudioSettings.sampleRate * mAudioSettings.numChannels * mAudioSettings.bitsPerSample / 8) / 100; // 10ms buffer
	std::vector<uint8_t> buffer(bufferSize);

	while (mCaptureRunning)
	{
		if (pa_simple_read(captureStream, buffer.data(), buffer.size(), &error) < 0)
		{
			TELEPORT_CERR << "LinuxAudioPlayer: Failed to read audio data: " << pa_strerror(error) << std::endl;
			break;
		}

		if (mRecordingCallback)
		{
			mRecordingCallback(buffer.data(), buffer.size());
		}
	}

	pa_simple_free(captureStream);
}

Result LinuxAudioPlayer::processRecordedAudio()
{
	if (!mRecording)
	{
		TELEPORT_COUT << "LinuxAudioPlayer: Not recording." << std::endl;
		return Result::AudioProcessingError;
	}
	// Audio is processed asynchronously in the capture thread
	return Result::OK;
}

Result LinuxAudioPlayer::stopRecording()
{
	if (!mRecording)
	{
		TELEPORT_COUT << "LinuxAudioPlayer: Not recording." << std::endl;
		return Result::OK;
	}

	mCaptureRunning = false;
	if (mCaptureThread.joinable())
	{
		mCaptureThread.join();
	}

	mRecording = false;
	return Result::OK;
}

Result LinuxAudioPlayer::deconfigure()
{
	if (!mConfigured)
	{
		TELEPORT_COUT << "LinuxAudioPlayer: Can't deconfigure audio player because it is not configured." << std::endl;
		return Result::AudioPlayerNotConfigured;
	}

	if (mRecording)
	{
		stopRecording();
	}

	mRecordingAllowed = false;
	mConfigured = false;
	mAudioSettings = {};

	if (mPlaybackStream)
	{
		int error;
		pa_simple_drain(mPlaybackStream, &error);
		pa_simple_free(mPlaybackStream);
		mPlaybackStream = nullptr;
	}

	return Result::OK;
}

void LinuxAudioPlayer::onAudioProcessed()
{
	mAudioBufferQueue.pop();
}

