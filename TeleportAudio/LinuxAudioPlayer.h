// (C) Copyright 2018-2024 Simul Software Ltd
#pragma once

#include "AudioPlayer.h"
#include <future>
#include <libavstream/common.hpp>

struct pa_simple;

namespace teleport
{
	namespace audio
	{
		/*! A class to play audio from streams and files for Linux using PulseAudio
		*/
		class LinuxAudioPlayer final : public AudioPlayer
		{
		public:
			LinuxAudioPlayer();
			~LinuxAudioPlayer();

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

			std::future<Result> mInitResult;

			// PulseAudio simple API for playback
			pa_simple* mPlaybackStream = nullptr;

			// Buffer queue for audio data
			avs::ThreadSafeQueue<std::vector<uint8_t>> mAudioBufferQueue;

			// Recording callback and thread
			std::function<void(const uint8_t* data, size_t dataSize)> mRecordingCallback;
			std::atomic<bool> mCaptureRunning{false};
			std::thread mCaptureThread;

			void captureThreadFunc();
		};
	}
}

