// (C) Copyright 2018-2020 Simul Software Ltd
#pragma once

#include "AudioCommon.h"
#include <functional>

namespace teleport
{
	namespace audio
	{
		/*! A class to play audio from streams and files
		*/
		class AudioPlayer
		{
		public:
			AudioPlayer();
			virtual ~AudioPlayer() = default;
			virtual Result initializeAudioDevice() = 0;
			virtual Result configure(const AudioSettings& audioSettings) = 0;
			virtual Result playStream(const uint8_t* data, size_t dataSize) = 0;
			virtual Result startRecording(std::function<void(const uint8_t* data, size_t dataSize)> recordingCallback) = 0;
			virtual Result processRecordedAudio() = 0;
			virtual Result stopRecording() = 0;
			virtual Result deconfigure() = 0;
			virtual void onAudioProcessed() = 0;
			bool isInitialized() const { return mInitialized; }
			bool isConfigured() const { return mConfigured; }
			bool isRecording() const { return mRecording; }
			bool isInputDeviceAvailable() const { return mInputDeviceAvailable; }

		protected:
			AudioSettings mAudioSettings;
			bool mInitialized;
			bool mConfigured;
			bool mRecording;
			bool mRecordingAllowed;
			bool mInputDeviceAvailable;
			Result mLastResult;
		};
	}
}
