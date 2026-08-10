// (C) Copyright 2018-2026 Simul Software Ltd

#include "MacAudioPlayer.h"
#include "TeleportCore/ErrorHandling.h"
#include <AudioToolbox/AudioToolbox.h>
#include <CoreAudio/CoreAudio.h>
#include <algorithm>
#include <cstring>

using namespace teleport::audio;

namespace
{
	constexpr OSStatus kAudioQueuePermissionDenied = -66672;
}

MacAudioPlayer::MacAudioPlayer()
{
	mRecordingAllowed = true;
}

MacAudioPlayer::~MacAudioPlayer()
{
	if (mConfigured)
	{
		deconfigure();
	}
}

Result MacAudioPlayer::initializeAudioDevice()
{
	if (mInitialized)
	{
		return Result::AudioPlayerAlreadyInitialized;
	}

	mInitResult = std::async(std::launch::async, &MacAudioPlayer::asyncInitializeAudioDevice, this);
	return Result::OK;
}

Result MacAudioPlayer::asyncInitializeAudioDevice()
{
	AudioObjectPropertyAddress propertyAddress =
	{
		kAudioHardwarePropertyDefaultInputDevice,
		kAudioObjectPropertyScopeGlobal,
		kAudioObjectPropertyElementMain
	};

	AudioDeviceID deviceID = kAudioObjectUnknown;
	UInt32 size = sizeof(deviceID);
	OSStatus status = AudioObjectGetPropertyData(kAudioObjectSystemObject, &propertyAddress, 0, nullptr, &size, &deviceID);
	if (status != noErr || deviceID == kAudioObjectUnknown)
	{
		TELEPORT_INTERNAL_COUT(Default, "MacAudioPlayer: No default input device found.");
		return Result::NoAudioInputDeviceFound;
	}

	return Result::OK;
}

AudioStreamBasicDescription MacAudioPlayer::MakeFormat(const AudioSettings& settings) const
{
	AudioStreamBasicDescription fmt = {};
	fmt.mSampleRate = static_cast<Float64>(settings.sampleRate);
	fmt.mFormatID = kAudioFormatLinearPCM;

	if (settings.bitsPerSample == 32)
	{
		fmt.mFormatFlags = kAudioFormatFlagIsFloat | kAudioFormatFlagIsPacked;
	}
	else
	{
		fmt.mFormatFlags = kAudioFormatFlagIsSignedInteger | kAudioFormatFlagIsPacked;
	}

	fmt.mBytesPerPacket = settings.numChannels * settings.bitsPerSample / 8;
	fmt.mFramesPerPacket = 1;
	fmt.mBytesPerFrame = fmt.mBytesPerPacket;
	fmt.mChannelsPerFrame = settings.numChannels;
	fmt.mBitsPerChannel = settings.bitsPerSample;
	fmt.mReserved = 0;
	return fmt;
}

Result MacAudioPlayer::configure(const AudioSettings& audioSettings)
{
	if (mConfigured)
	{
		TELEPORT_INTERNAL_COUT(Default, "MacAudioPlayer: Audio player has already been configured.");
		return Result::AudioPlayerAlreadyConfigured;
	}

	if (!mInitialized)
	{
		Result res = initializeAudioDevice();
		if (res == Result::OK)
		{
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

	if (audioSettings.codec != AudioCodec::PCM && audioSettings.codec != AudioCodec::Any)
	{
		TELEPORT_INTERNAL_CERR("MacAudioPlayer: Unsupported codec.");
		return Result::AudioStreamConfigurationError;
	}

	if (audioSettings.bitsPerSample != 16 && audioSettings.bitsPerSample != 32)
	{
		TELEPORT_INTERNAL_CERR("MacAudioPlayer: Unsupported bits per sample: {}", audioSettings.bitsPerSample);
		return Result::AudioStreamConfigurationError;
	}

	if (audioSettings.numChannels < 1 || audioSettings.numChannels > 2)
	{
		TELEPORT_INTERNAL_CERR("MacAudioPlayer: Unsupported channel count: {}", audioSettings.numChannels);
		return Result::AudioStreamConfigurationError;
	}

	const uint32_t sampleSize = audioSettings.numChannels * audioSettings.bitsPerSample / 8;
	if (sampleSize == 0)
	{
		return Result::AudioStreamConfigurationError;
	}

	mBufferByteSize = (static_cast<size_t>(audioSettings.sampleRate) * sampleSize) / 50;
	if (mBufferByteSize == 0)
	{
		return Result::AudioStreamConfigurationError;
	}

	mAudioSettings = audioSettings;
	mConfigured = true;
	return Result::OK;
}

void MacAudioPlayer::CreateOutputQueue()
{
	if (mOutputQueue)
	{
		return;
	}

	AudioStreamBasicDescription fmt = MakeFormat(mAudioSettings);
	OSStatus status = AudioQueueNewOutput(&fmt, OutputCallback, this, nullptr, nullptr, 0, &mOutputQueue);
	if (status != noErr || !mOutputQueue)
	{
		TELEPORT_INTERNAL_CERR("MacAudioPlayer: Failed to create output queue.");
		mOutputQueue = nullptr;
		return;
	}

	AudioQueueSetParameter(mOutputQueue, kAudioQueueParam_Volume, static_cast<Float32>(mVolume));

	for (size_t i = 0; i < sNumBuffers; ++i)
	{
		AudioQueueBufferRef buffer = nullptr;
		status = AudioQueueAllocateBuffer(mOutputQueue, static_cast<UInt32>(mBufferByteSize), &buffer);
		if (status != noErr || !buffer)
		{
			TELEPORT_INTERNAL_CERR("MacAudioPlayer: Failed to allocate output buffer.");
			DestroyQueues();
			return;
		}
		buffer->mAudioDataByteSize = 0;
		mFreeOutputBuffers.push_back(buffer);
	}
}

void MacAudioPlayer::CreateInputQueue()
{
	if (mInputQueue)
	{
		return;
	}

	AudioStreamBasicDescription fmt = MakeFormat(mAudioSettings);
	OSStatus status = AudioQueueNewInput(&fmt, InputCallback, this, nullptr, nullptr, 0, &mInputQueue);
	if (status != noErr || !mInputQueue)
	{
		TELEPORT_INTERNAL_CERR("MacAudioPlayer: Failed to create input queue.");
		mInputQueue = nullptr;
		return;
	}

	for (size_t i = 0; i < sNumBuffers; ++i)
	{
		AudioQueueBufferRef buffer = nullptr;
		status = AudioQueueAllocateBuffer(mInputQueue, static_cast<UInt32>(mBufferByteSize), &buffer);
		if (status != noErr || !buffer)
		{
			TELEPORT_INTERNAL_CERR("MacAudioPlayer: Failed to allocate input buffer.");
			DestroyQueues();
			return;
		}

		status = AudioQueueEnqueueBuffer(mInputQueue, buffer, 0, nullptr);
		if (status != noErr)
		{
			TELEPORT_INTERNAL_CERR("MacAudioPlayer: Failed to enqueue input buffer.");
			DestroyQueues();
			return;
		}
	}
}

void MacAudioPlayer::DestroyQueues()
{
	if (mOutputQueue)
	{
		if (mOutputStarted)
		{
			AudioQueueStop(mOutputQueue, true);
			mOutputStarted = false;
		}
		AudioQueueDispose(mOutputQueue, true);
		mOutputQueue = nullptr;
	}

	{
		std::lock_guard<std::mutex> lock(mMutex);
		mFreeOutputBuffers.clear();
		mPendingChunks.clear();
	}

	if (mInputQueue)
	{
		if (mInputStarted)
		{
			AudioQueueStop(mInputQueue, true);
			mInputStarted = false;
		}
		AudioQueueDispose(mInputQueue, true);
		mInputQueue = nullptr;
	}
}

void MacAudioPlayer::SplitAndQueueStream(const uint8_t* data, size_t dataSize)
{
	const uint32_t sampleSize = mAudioSettings.numChannels * mAudioSettings.bitsPerSample / 8;
	if (sampleSize == 0 || dataSize == 0)
	{
		return;
	}

	std::lock_guard<std::mutex> lock(mMutex);
	size_t offset = 0;
	while (offset < dataSize)
	{
		size_t remaining = dataSize - offset;
		size_t chunkSize = std::min(remaining, mBufferByteSize);
		chunkSize -= chunkSize % sampleSize;
		if (chunkSize == 0)
		{
			break;
		}
		mPendingChunks.emplace_back(data + offset, data + offset + chunkSize);
		offset += chunkSize;
	}
}

void MacAudioPlayer::RefillOutputBuffers()
{
	std::lock_guard<std::mutex> lock(mMutex);
	RefillOutputBuffersLocked();
}

void MacAudioPlayer::RefillOutputBuffersLocked()
{
	const uint32_t sampleSize = mAudioSettings.numChannels * mAudioSettings.bitsPerSample / 8;
	if (sampleSize == 0 || !mOutputQueue)
	{
		return;
	}

	while (!mFreeOutputBuffers.empty() && !mPendingChunks.empty())
	{
		AudioQueueBufferRef buffer = mFreeOutputBuffers.front();
		mFreeOutputBuffers.pop_front();

		std::vector<uint8_t>& chunk = mPendingChunks.front();
		size_t copySize = std::min(chunk.size(), static_cast<size_t>(buffer->mAudioDataBytesCapacity));
		copySize -= copySize % sampleSize;

		if (copySize > 0)
		{
			std::memcpy(buffer->mAudioData, chunk.data(), copySize);
		}
		buffer->mAudioDataByteSize = static_cast<UInt32>(copySize);

		OSStatus status = AudioQueueEnqueueBuffer(mOutputQueue, buffer, 0, nullptr);
		if (status != noErr)
		{
			TELEPORT_INTERNAL_CERR("MacAudioPlayer: Failed to enqueue output buffer.");
			mFreeOutputBuffers.push_back(buffer);
			break;
		}

		if (copySize == chunk.size())
		{
			mPendingChunks.pop_front();
		}
		else
		{
			chunk.erase(chunk.begin(), chunk.begin() + static_cast<std::ptrdiff_t>(copySize));
		}

		if (!mOutputStarted)
		{
			AudioQueueStart(mOutputQueue, nullptr);
			mOutputStarted = true;
		}
	}
}

Result MacAudioPlayer::playStream(const uint8_t* data, size_t dataSize)
{
	if (!mInitialized)
	{
		TELEPORT_INTERNAL_CERR("MacAudioPlayer: Can't play audio stream because the audio player has not been initialized.");
		return Result::AudioPlayerNotInitialized;
	}

	if (!mConfigured)
	{
		static bool done = false;
		if (!done)
		{
			TELEPORT_INTERNAL_CERR("MacAudioPlayer: Can't play audio stream because the audio player has not been configured.");
			done = true;
		}
		return Result::AudioPlayerNotConfigured;
	}

	if (!mOutputQueue)
	{
		CreateOutputQueue();
		if (!mOutputQueue)
		{
			return Result::AudioStreamCreationError;
		}
	}

	AudioQueueSetParameter(mOutputQueue, kAudioQueueParam_Volume, static_cast<Float32>(mVolume));

	if (dataSize > 0)
	{
		SplitAndQueueStream(data, dataSize);
		RefillOutputBuffers();
	}

	return Result::OK;
}

void MacAudioPlayer::OutputCallback(void* inUserData, AudioQueueRef /*inAQ*/, AudioQueueBufferRef inBuffer)
{
	MacAudioPlayer* player = reinterpret_cast<MacAudioPlayer*>(inUserData);
	if (!player)
	{
		return;
	}

	{
		std::lock_guard<std::mutex> lock(player->mMutex);
		inBuffer->mAudioDataByteSize = 0;
		player->mFreeOutputBuffers.push_back(inBuffer);
	}
	player->RefillOutputBuffers();
}

Result MacAudioPlayer::startRecording(std::function<void(const uint8_t* data, size_t dataSize)> recordingCallback)
{
	if (!mInitialized)
	{
		TELEPORT_INTERNAL_CERR("MacAudioPlayer: Can't record audio because the audio player has not been initialized.");
		return Result::AudioPlayerNotInitialized;
	}

	if (!mConfigured)
	{
		TELEPORT_INTERNAL_CERR("MacAudioPlayer: Can't record audio because the audio player has not been configured.");
		return Result::AudioPlayerNotConfigured;
	}

	if (!mRecordingAllowed)
	{
		TELEPORT_INTERNAL_CERR("MacAudioPlayer: The user has not granted permission to record audio.");
		return Result::AudioRecordingNotPermitted;
	}

	if (mRecording)
	{
		TELEPORT_INTERNAL_COUT(Default, "MacAudioPlayer: Already recording.");
		return Result::OK;
	}

	if (!mInputQueue)
	{
		CreateInputQueue();
		if (!mInputQueue)
		{
			return Result::AudioRecorderCreationError;
		}
	}

	mRecordingCallback = recordingCallback;
	mRecording = true;

	OSStatus status = AudioQueueStart(mInputQueue, nullptr);
	if (status != noErr)
	{
		mRecording = false;
		TELEPORT_INTERNAL_CERR("MacAudioPlayer: Failed to start recording queue.");
		if (status == kAudioQueuePermissionDenied)
		{
			return Result::AudioRecordingNotPermitted;
		}
		return Result::AudioRecorderStartError;
	}

	mInputStarted = true;
	return Result::OK;
}

void MacAudioPlayer::InputCallback(void* inUserData, AudioQueueRef inAQ, AudioQueueBufferRef inBuffer,
	const AudioTimeStamp* /*inStartTime*/, UInt32 /*inNumberPacketDescriptions*/,
	const AudioStreamPacketDescription* /*inPacketDescs*/)
{
	MacAudioPlayer* player = reinterpret_cast<MacAudioPlayer*>(inUserData);
	if (!player)
	{
		return;
	}

	if (player->mRecording && player->mRecordingCallback)
	{
		player->mRecordingCallback(reinterpret_cast<const uint8_t*>(inBuffer->mAudioData), inBuffer->mAudioDataByteSize);
	}

	OSStatus status = AudioQueueEnqueueBuffer(inAQ, inBuffer, 0, nullptr);
	if (status != noErr)
	{
		TELEPORT_INTERNAL_CERR("MacAudioPlayer: Failed to reuse input buffer.");
	}
}

Result MacAudioPlayer::processRecordedAudio()
{
	if (!mRecording)
	{
		TELEPORT_INTERNAL_COUT(Default, "MacAudioPlayer: Not recording.");
		return Result::AudioProcessingError;
	}

	return Result::OK;
}

Result MacAudioPlayer::stopRecording()
{
	if (!mRecording)
	{
		TELEPORT_INTERNAL_COUT(Default, "MacAudioPlayer: Not recording.");
		return Result::OK;
	}

	mRecording = false;
	mRecordingCallback = nullptr;

	if (mInputQueue && mInputStarted)
	{
		AudioQueueStop(mInputQueue, true);
		mInputStarted = false;
	}

	return Result::OK;
}

Result MacAudioPlayer::deconfigure()
{
	if (!mConfigured)
	{
		TELEPORT_INTERNAL_COUT(Default, "MacAudioPlayer: Can't deconfigure audio player because it is not configured.");
		return Result::AudioPlayerNotConfigured;
	}

	if (mRecording)
	{
		stopRecording();
	}

	DestroyQueues();

	mRecordingAllowed = false;
	mConfigured = false;
	mAudioSettings = {};

	return Result::OK;
}

void MacAudioPlayer::onAudioProcessed()
{
	// Buffers are recycled by the AudioQueue output callback.
}
