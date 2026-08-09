// (C) Copyright 2018-2026 Simul Software Ltd
#pragma once

#include "AudioPlayer.h"
#include <AudioToolbox/AudioQueue.h>
#include <deque>
#include <functional>
#include <future>
#include <mutex>
#include <vector>

namespace teleport
{
	namespace audio
	{
		/*! CoreAudio AudioQueue backend for macOS. Provides playback of decoded
			spatial audio (stereo, 48 kHz, 16-bit PCM) and capture of microphone input
			(mono, 48 kHz, 16-bit PCM).
		*/
		class MacAudioPlayer final : public AudioPlayer
		{
		public:
			MacAudioPlayer();
			~MacAudioPlayer();

			Result initializeAudioDevice() override;
			Result configure(const AudioSettings& audioSettings) override;
			Result playStream(const uint8_t* data, size_t dataSize) override;
			Result startRecording(std::function<void(const uint8_t* data, size_t dataSize)> recordingCallback) override;
			Result processRecordedAudio() override;
			Result stopRecording() override;
			Result deconfigure() override;
			void onAudioProcessed() override;

		private:
			Result asyncInitializeAudioDevice();

			AudioStreamBasicDescription MakeFormat(const AudioSettings& settings) const;
			void CreateOutputQueue();
			void CreateInputQueue();
			void DestroyQueues();

			void SplitAndQueueStream(const uint8_t* data, size_t dataSize);
			void RefillOutputBuffers();
			void RefillOutputBuffersLocked();

			static void OutputCallback(void* inUserData, AudioQueueRef inAQ, AudioQueueBufferRef inBuffer);
			static void InputCallback(void* inUserData, AudioQueueRef inAQ, AudioQueueBufferRef inBuffer,
				const AudioTimeStamp* inStartTime, UInt32 inNumberPacketDescriptions,
				const AudioStreamPacketDescription* inPacketDescs);

			std::future<Result> mInitResult;

			AudioQueueRef mOutputQueue = nullptr;
			AudioQueueRef mInputQueue = nullptr;
			bool mOutputStarted = false;
			bool mInputStarted = false;

			static constexpr size_t sNumBuffers = 3;
			size_t mBufferByteSize = 0;

			std::mutex mMutex;
			std::deque<AudioQueueBufferRef> mFreeOutputBuffers;
			std::deque<std::vector<uint8_t>> mPendingChunks;

			std::function<void(const uint8_t* data, size_t dataSize)> mRecordingCallback;
			std::atomic<bool> mRecording{ false };
		};
	}
}
