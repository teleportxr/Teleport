// (C) Copyright 2018-2026 Simul Software Ltd
#pragma once

#include "AudioPlayer.h"

namespace teleport
{
	namespace audio
	{
		/*! No-op audio backend for macOS. There is no CoreAudio implementation yet - this
			exists purely so ClientRender links and runs; audio is silently discarded.
		*/
		class MacAudioPlayer final : public AudioPlayer
		{
		public:
			MacAudioPlayer()  = default;
			~MacAudioPlayer() = default;

			Result initializeAudioDevice() override
			{
				return Result();
			}

			Result configure(const AudioSettings &audioSettings) override
			{
				mAudioSettings = audioSettings;
				return Result();
			}

			Result playStream(const uint8_t *data, size_t dataSize) override
			{
				return Result();
			}

			Result startRecording(std::function<void(const uint8_t *data, size_t dataSize)> recordingCallback) override
			{
				return Result();
			}

			Result processRecordedAudio() override
			{
				return Result();
			}

			Result stopRecording() override
			{
				return Result();
			}

			Result deconfigure() override
			{
				return Result();
			}

			void onAudioProcessed() override
			{
			}
		};
	}
}
